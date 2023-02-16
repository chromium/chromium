// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

constexpr char kProfileName[] = "user@gmail.com";
constexpr char kGaiaId[] = "1234567890";

constexpr char kUserActionUpdate[] = "update";

constexpr int64_t kFreeDiskSpaceLessThanThreshold = 1LL << 29;
constexpr int64_t kFreeDiskSpaceMoreThanThreshold = 1LL << 31;
constexpr double kBatteryPercentLessThanThreshold = 20.0;
constexpr double kBatteryPercentMoreThanThreshold = 40.0;

// Fake WakeLock implementation to allow ArcVmDataMigrationScreen to check
// whether it holds WakeLock.
class FakeWakeLock : public device::mojom::WakeLock {
 public:
  FakeWakeLock() = default;
  ~FakeWakeLock() override = default;
  FakeWakeLock(const FakeWakeLock&) = delete;
  FakeWakeLock& operator=(const FakeWakeLock&) = delete;

  void RequestWakeLock() override { has_wakelock_ = true; }
  void CancelWakeLock() override { has_wakelock_ = false; }
  void AddClient(
      mojo::PendingReceiver<device::mojom::WakeLock> receiver) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {}
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {}

  bool HasWakeLock() { return has_wakelock_; }

 private:
  bool has_wakelock_ = false;
};

// Fake ArcVmDataMigrationScreenView implementation to expose the UI state and
// free disk space / battery state info sent from ArcVmDataMigrationScreen.
class FakeArcVmDataMigrationScreenView : public ArcVmDataMigrationScreenView {
 public:
  FakeArcVmDataMigrationScreenView() = default;
  ~FakeArcVmDataMigrationScreenView() override = default;
  FakeArcVmDataMigrationScreenView(const FakeArcVmDataMigrationScreenView&) =
      delete;
  FakeArcVmDataMigrationScreenView& operator=(
      const ArcVmDataMigrationScreenView&) = delete;

  bool shown() { return shown_; }
  UIState state() { return state_; }
  bool minimum_free_disk_space_set() { return minimum_free_disk_space_set_; }
  bool minimum_battery_percent_set() { return minimum_battery_percent_set_; }
  bool has_enough_free_disk_space() { return has_enough_free_disk_space_; }
  bool has_enough_battery() { return has_enough_battery_; }
  bool is_connected_to_charger() { return is_connected_to_charger_; }

 private:
  void Show() override { shown_ = true; }
  void SetUIState(UIState state) override { state_ = state; }
  void SetRequiredFreeDiskSpace(int64_t required_free_disk_space) override {
    minimum_free_disk_space_set_ = true;
    has_enough_free_disk_space_ = false;
  }
  void SetMinimumBatteryPercent(double percent) override {
    minimum_battery_percent_set_ = true;
  }

  void SetBatteryState(bool enough, bool connected) override {
    has_enough_battery_ = enough;
    is_connected_to_charger_ = connected;
  }

  bool shown_ = false;
  UIState state_ = UIState::kLoading;
  bool minimum_free_disk_space_set_ = false;
  bool minimum_battery_percent_set_ = false;
  bool has_enough_free_disk_space_ = true;
  bool has_enough_battery_ = false;
  bool is_connected_to_charger_ = false;
};

// Fake ArcVmDataMigrationScreen that exposes whether it has encountered a fatal
// error. It also exposes whether it has WakeLock.
class TestArcVmDataMigrationScreen : public ArcVmDataMigrationScreen {
 public:
  explicit TestArcVmDataMigrationScreen(
      base::WeakPtr<ArcVmDataMigrationScreenView> view)
      : ArcVmDataMigrationScreen(std::move(view)) {}

  bool encountered_fatal_error() { return encountered_fatal_error_; }
  bool HasWakeLock() { return fake_wake_lock_.HasWakeLock(); }

 private:
  void HandleFatalError() override { encountered_fatal_error_ = true; }
  device::mojom::WakeLock* GetWakeLock() override { return &fake_wake_lock_; }

  bool encountered_fatal_error_ = false;
  FakeWakeLock fake_wake_lock_;
};

class ArcVmDataMigrationScreenTest : public ChromeAshTestBase,
                                     public ConciergeClient::VmObserver {
 public:
  ArcVmDataMigrationScreenTest() = default;
  ~ArcVmDataMigrationScreenTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();

    ConciergeClient::InitializeFake();
    UpstartClient::InitializeFake();
    SpacedClient::InitializeFake();

    wizard_context_ = std::make_unique<WizardContext>();

    // Set up a primary profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(kProfileName);
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), kGaiaId);
    auto fake_user_manager = std::make_unique<FakeChromeUserManager>();
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
    DCHECK(ash::ProfileHelper::IsPrimaryProfile(profile_));

    // Set the default states. They can be overwritten by individual test cases.
    arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                     arc::ArcVmDataMigrationStatus::kConfirmed);
    SetFreeDiskSpace(/*enough=*/true);
    SetBatteryState(/*enough=*/true, /*connected=*/true);

    view_ = std::make_unique<FakeArcVmDataMigrationScreenView>();
    screen_ = std::make_unique<TestArcVmDataMigrationScreen>(
        view_.get()->AsWeakPtr());

    vm_observation_.Observe(FakeConciergeClient::Get());
  }

  void TearDown() override {
    vm_observation_.Reset();

    screen_.reset();
    view_.reset();

    user_manager_.reset();
    profile_manager_->DeleteTestingProfile(kProfileName);
    profile_ = nullptr;
    profile_manager_.reset();

    wizard_context_.reset();

    SpacedClient::Shutdown();
    UpstartClient::Shutdown();
    ConciergeClient::Shutdown();

    ChromeAshTestBase::TearDown();
  }

 protected:
  void SetFreeDiskSpace(bool enough) {
    FakeSpacedClient::Get()->set_free_disk_space(
        enough ? kFreeDiskSpaceMoreThanThreshold
               : kFreeDiskSpaceLessThanThreshold);
  }

  void SetBatteryState(bool enough, bool connected) {
    power_manager::PowerSupplyProperties props;
    props.set_battery_percent(enough ? kBatteryPercentMoreThanThreshold
                                     : kBatteryPercentLessThanThreshold);
    if (connected) {
      props.set_external_power(
          power_manager::PowerSupplyProperties_ExternalPower_AC);
    } else {
      props.set_external_power(
          power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED);
    }

    chromeos::FakePowerManagerClient::Get()->UpdatePowerProperties(props);
  }

  void PressUpdateButton() {
    base::Value::List args;
    args.Append(kUserActionUpdate);
    screen_->HandleUserAction(args);
  }

  // FakeConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {}
  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() == arc::kArcVmName) {
      arc_vm_stopped_ = true;
    }
  }

  bool arc_vm_stopped_ = false;

  std::unique_ptr<WizardContext> wizard_context_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_ = nullptr;  // Owned by |profile_manager_|.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;

  std::unique_ptr<TestArcVmDataMigrationScreen> screen_;
  std::unique_ptr<FakeArcVmDataMigrationScreenView> view_;

  base::ScopedObservation<ConciergeClient, ConciergeClient::VmObserver>
      vm_observation_{this};
};

TEST_F(ArcVmDataMigrationScreenTest, ScreenTransition) {
  EXPECT_FALSE(view_->shown());
  screen_->Show(wizard_context_.get());
  EXPECT_TRUE(view_->shown());
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kLoading);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
}

TEST_F(ArcVmDataMigrationScreenTest, NotEnoughDiskSpace) {
  SetFreeDiskSpace(/*enough=*/false);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_TRUE(view_->minimum_free_disk_space_set());
  EXPECT_FALSE(view_->has_enough_free_disk_space());
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, BatteryStateUpdate_InitiallyGood) {
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_TRUE(view_->minimum_battery_percent_set());
  EXPECT_TRUE(view_->has_enough_battery());
  EXPECT_TRUE(view_->is_connected_to_charger());

  SetBatteryState(/*enough=*/true, /*connected=*/false);
  EXPECT_TRUE(view_->has_enough_battery());
  EXPECT_FALSE(view_->is_connected_to_charger());

  SetBatteryState(/*enough=*/false, /*connected=*/false);
  EXPECT_FALSE(view_->has_enough_battery());
  EXPECT_FALSE(view_->is_connected_to_charger());

  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, BatteryStateUpdate_InitiallyBad) {
  SetBatteryState(/*enough=*/false, /*connected=*/false);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_TRUE(view_->minimum_battery_percent_set());
  EXPECT_FALSE(view_->has_enough_battery());
  EXPECT_FALSE(view_->is_connected_to_charger());

  SetBatteryState(/*enough=*/false, /*connected=*/true);
  EXPECT_FALSE(view_->has_enough_battery());
  EXPECT_TRUE(view_->is_connected_to_charger());

  SetBatteryState(/*enough=*/true, /*connected=*/true);
  EXPECT_TRUE(view_->has_enough_battery());
  EXPECT_TRUE(view_->is_connected_to_charger());

  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, WakeLockIsHeldWhileScreenIsShown) {
  EXPECT_FALSE(screen_->HasWakeLock());
  screen_->Show(wizard_context_.get());
  EXPECT_TRUE(screen_->HasWakeLock());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->HasWakeLock());
  screen_->Hide();
  EXPECT_FALSE(screen_->HasWakeLock());
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, ScreenLockIsDisabledWhileScreenIsShown) {
  auto* session_controller = Shell::Get()->session_controller();
  EXPECT_TRUE(session_controller->CanLockScreen());
  screen_->Show(wizard_context_.get());
  EXPECT_FALSE(session_controller->CanLockScreen());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(session_controller->CanLockScreen());
  screen_->Hide();
  EXPECT_TRUE(session_controller->CanLockScreen());
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, GetVmInfoFailureIsFatal) {
  FakeConciergeClient::Get()->set_get_vm_info_response(absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, ArcVmNotRunning) {
  auto* fake_concierge_client = FakeConciergeClient::Get();
  vm_tools::concierge::GetVmInfoResponse get_vm_info_response;
  // Unsuccessful response means that the VM is not running.
  get_vm_info_response.set_success(false);
  fake_concierge_client->set_get_vm_info_response(get_vm_info_response);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_EQ(fake_concierge_client->stop_vm_call_count(), 0);
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, StopArcVmFailureIsFatal) {
  auto* fake_concierge_client = FakeConciergeClient::Get();
  vm_tools::concierge::GetVmInfoResponse get_vm_info_response;
  get_vm_info_response.set_success(true);
  fake_concierge_client->set_get_vm_info_response(get_vm_info_response);
  fake_concierge_client->set_stop_vm_response(absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, StopArcVmSuccess) {
  auto* fake_concierge_client = FakeConciergeClient::Get();
  vm_tools::concierge::GetVmInfoResponse get_vm_info_response;
  get_vm_info_response.set_success(true);
  fake_concierge_client->set_get_vm_info_response(get_vm_info_response);
  vm_tools::concierge::StopVmResponse stop_vm_response;
  stop_vm_response.set_success(true);
  fake_concierge_client->set_stop_vm_response(stop_vm_response);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_EQ(fake_concierge_client->stop_vm_call_count(), 1);
  EXPECT_TRUE(arc_vm_stopped_);
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

// Tests that UpstartClient::StopJob() is called for each job to be stopped even
// when it fails for some of them; we do not treat failures of StopJob() as
// fatal because it returns an unsuccessful response when the target job is not
// running.
TEST_F(ArcVmDataMigrationScreenTest, StopArcUpstartJobs) {
  std::set<std::string> jobs_to_be_stopped(
      std::begin(arc::kArcVmUpstartJobsToBeStoppedOnRestart),
      std::end(arc::kArcVmUpstartJobsToBeStoppedOnRestart));
  FakeUpstartClient::Get()->set_stop_job_cb(base::BindLambdaForTesting(
      [&jobs_to_be_stopped](const std::string& job_name,
                            const std::vector<std::string>& env) {
        EXPECT_TRUE(jobs_to_be_stopped.contains(job_name));
        jobs_to_be_stopped.erase(job_name);
        return (jobs_to_be_stopped.size() % 2) == 0;
      }));

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_TRUE(jobs_to_be_stopped.empty());
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, CreateDiskImageSuccess) {
  // CreateDiskImageResponse is set to DISK_STATUS_CREATED by default.
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_EQ(FakeConciergeClient::Get()->create_disk_image_call_count(), 1);
  EXPECT_FALSE(screen_->encountered_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, CreateDiskImageFailureIsFatal) {
  vm_tools::concierge::CreateDiskImageResponse response;
  response.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  FakeConciergeClient::Get()->set_create_disk_image_response(response);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_fatal_error());
}

}  // namespace
}  // namespace ash
