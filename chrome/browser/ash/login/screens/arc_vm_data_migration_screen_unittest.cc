// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arc/fake_arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"
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

constexpr char kArcRemoveDataJobName[] = "arc_2dremove_2ddata";

constexpr char kProfileName[] = "user@gmail.com";
constexpr char kGaiaId[] = "1234567890";

constexpr char kUserActionUpdate[] = "update";
constexpr char kUserActionResume[] = "resume";

constexpr uint64_t kDefaultAndroidDataSize = 8ULL << 30;

// These values assume the default Android /data size.
constexpr uint64_t kFreeDiskSpaceLessThanThreshold = 512ULL << 20;
constexpr uint64_t kFreeDiskSpaceMoreThanThreshold = 2ULL << 30;

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
  double migration_progress() { return migration_progress_; }
  base::TimeDelta estimated_remaining_time() {
    return estimated_remaining_time_;
  }

 private:
  void Show() override { shown_ = true; }
  void SetUIState(UIState state) override { state_ = state; }
  void SetRequiredFreeDiskSpace(uint64_t required_free_disk_space) override {
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

  void SetMigrationProgress(double progress) override {
    migration_progress_ = progress;
  }

  void SetEstimatedRemainingTime(const base::TimeDelta& delta) override {
    estimated_remaining_time_ = delta;
  }

  bool shown_ = false;
  UIState state_ = UIState::kLoading;
  bool minimum_free_disk_space_set_ = false;
  bool minimum_battery_percent_set_ = false;
  bool has_enough_free_disk_space_ = true;
  bool has_enough_battery_ = false;
  bool is_connected_to_charger_ = false;
  double migration_progress_ = 0.0;
  base::TimeDelta estimated_remaining_time_ = base::TimeDelta();
};

// Fake ArcVmDataMigrationScreen that exposes whether it has encountered a fatal
// error. It also exposes whether it has WakeLock.
class TestArcVmDataMigrationScreen : public ArcVmDataMigrationScreen {
 public:
  explicit TestArcVmDataMigrationScreen(
      base::WeakPtr<ArcVmDataMigrationScreenView> view)
      : ArcVmDataMigrationScreen(std::move(view)) {}

  bool encountered_retriable_fatal_error() {
    return encountered_retriable_fatal_error_;
  }
  bool HasWakeLock() { return fake_wake_lock_.HasWakeLock(); }

 private:
  void HandleRetriableFatalError() override {
    encountered_retriable_fatal_error_ = true;
  }
  device::mojom::WakeLock* GetWakeLock() override { return &fake_wake_lock_; }

  bool encountered_retriable_fatal_error_ = false;
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
    ArcVmDataMigratorClient::InitializeFake();

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
    arc::data_migrator::GetAndroidDataInfoResponse response;
    response.set_total_allocated_space_src(kDefaultAndroidDataSize);
    response.set_total_allocated_space_dest(kDefaultAndroidDataSize);
    FakeArcVmDataMigratorClient::Get()->set_get_android_data_info_response(
        response);
    SetFreeDiskSpace(/*enough=*/true);
    SetBatteryState(/*enough=*/true, /*connected=*/true);

    view_ = std::make_unique<FakeArcVmDataMigrationScreenView>();
    screen_ = std::make_unique<TestArcVmDataMigrationScreen>(
        view_.get()->AsWeakPtr());
    screen_->SetTickClockForTesting(&tick_clock_);

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

    ArcVmDataMigratorClient::Shutdown();
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

  void PressResumeButton() {
    base::Value::List args;
    args.Append(kUserActionResume);
    screen_->HandleUserAction(args);
  }

  void SendDataMigrationProgress(uint64_t current_bytes, uint64_t total_bytes) {
    arc::data_migrator::DataMigrationProgress progress;
    progress.set_status(
        arc::data_migrator::DataMigrationStatus::DATA_MIGRATION_IN_PROGRESS);
    progress.set_current_bytes(current_bytes);
    progress.set_total_bytes(total_bytes);
    FakeArcVmDataMigratorClient::Get()->SendDataMigrationProgress(progress);
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

  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<WizardContext> wizard_context_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, ExperimentalAsh> profile_ =
      nullptr;  // Owned by |profile_manager_|.
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
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
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

  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
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

  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, WakeLockIsHeldWhileScreenIsShown) {
  EXPECT_FALSE(screen_->HasWakeLock());
  screen_->Show(wizard_context_.get());
  EXPECT_TRUE(screen_->HasWakeLock());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->HasWakeLock());
  screen_->Hide();
  EXPECT_FALSE(screen_->HasWakeLock());
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
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
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, GetVmInfoFailureIsFatal) {
  FakeConciergeClient::Get()->set_get_vm_info_response(absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
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
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, StopArcVmFailureIsFatal) {
  auto* fake_concierge_client = FakeConciergeClient::Get();
  vm_tools::concierge::GetVmInfoResponse get_vm_info_response;
  get_vm_info_response.set_success(true);
  fake_concierge_client->set_get_vm_info_response(get_vm_info_response);
  fake_concierge_client->set_stop_vm_response(absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
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
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
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
        // Do not check the existence of the job in |job_to_be_stopped|, because
        // some jobs can be stopped multiple times.
        jobs_to_be_stopped.erase(job_name);
        return (jobs_to_be_stopped.size() % 2) == 0;
      }));

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kWelcome);
  EXPECT_TRUE(jobs_to_be_stopped.empty());
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest,
       ArcVmDataMigratorStartFailureOnGetAndroidDataSizeIsFatal) {
  FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return FakeUpstartClient::StartJobResult(
            job_name != arc::kArcVmDataMigratorJobName);
      }));

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest,
       ArcVmDataMigratorStartFailureOnStartMigrateIsFatal) {
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return FakeUpstartClient::StartJobResult(
            job_name != arc::kArcVmDataMigratorJobName);
      }));

  PressUpdateButton();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, GetAndroidDataSizeFailureIsFatal) {
  FakeArcVmDataMigratorClient::Get()->set_get_android_data_info_response(
      absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, CreateDiskImageSuccess) {
  // CreateDiskImageResponse is set to DISK_STATUS_CREATED by default.
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kProgress);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kStarted);
  EXPECT_EQ(FakeConciergeClient::Get()->create_disk_image_call_count(), 1);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, CreateDiskImageFailureIsFatal) {
  vm_tools::concierge::CreateDiskImageResponse response;
  response.set_status(vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED);
  FakeConciergeClient::Get()->set_create_disk_image_response(response);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, MigrationInProgress) {
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();

  SendDataMigrationProgress(0, 400);
  tick_clock_.Advance(base::Seconds(1));
  SendDataMigrationProgress(20, 400);
  // Note that here we rely on the precision of floating point calculation.
  EXPECT_EQ(static_cast<int>(std::round(view_->migration_progress())), 5);
  EXPECT_EQ(view_->estimated_remaining_time().InMilliseconds(), 19000);
  tick_clock_.Advance(base::Seconds(2));
  SendDataMigrationProgress(40, 400);
  EXPECT_EQ(static_cast<int>(std::round(view_->migration_progress())), 10);
  EXPECT_EQ(view_->estimated_remaining_time().InMilliseconds(), 18947);
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kProgress);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kStarted);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, Resume) {
  arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                   arc::ArcVmDataMigrationStatus::kStarted);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kResume);

  PressResumeButton();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kProgress);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kStarted);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest,
       SetupFailureIsTreatedAsMigrationFailureOnLoadingResumeScreen) {
  arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                   arc::ArcVmDataMigrationStatus::kStarted);
  // Assume that somehow Concierge cannot check whether ARCVM is running.
  FakeConciergeClient::Get()->set_get_vm_info_response(absl::nullopt);

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kFailure);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kFinished);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest,
       SetupFailureIsTreatedAsMigrationFailureOnResumeScreen) {
  arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                   arc::ArcVmDataMigrationStatus::kStarted);
  // Let ArcVmDataMigrator fail to start.
  FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return FakeUpstartClient::StartJobResult(
            job_name != arc::kArcVmDataMigratorJobName);
      }));

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();
  PressResumeButton();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kFailure);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kFinished);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, MigrationSuccess) {
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();

  arc::data_migrator::DataMigrationProgress progress;
  progress.set_status(
      arc::data_migrator::DataMigrationStatus::DATA_MIGRATION_SUCCESS);
  FakeArcVmDataMigratorClient::Get()->SendDataMigrationProgress(progress);
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kSuccess);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kFinished);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, MigrationFailure) {
  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();

  arc::data_migrator::DataMigrationProgress progress;
  progress.set_status(
      arc::data_migrator::DataMigrationStatus::DATA_MIGRATION_FAILED);
  FakeArcVmDataMigratorClient::Get()->SendDataMigrationProgress(progress);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kFailure);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kFinished);
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

TEST_F(ArcVmDataMigrationScreenTest, MigrationFailure_ArcDataRemovalFailed) {
  FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        return FakeUpstartClient::StartJobResult(job_name !=
                                                 kArcRemoveDataJobName);
      }));

  screen_->Show(wizard_context_.get());
  task_environment()->RunUntilIdle();

  PressUpdateButton();
  task_environment()->RunUntilIdle();

  arc::data_migrator::DataMigrationProgress progress;
  progress.set_status(
      arc::data_migrator::DataMigrationStatus::DATA_MIGRATION_FAILED);
  FakeArcVmDataMigratorClient::Get()->SendDataMigrationProgress(progress);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(view_->state(), ArcVmDataMigrationScreenView::UIState::kFailure);
  EXPECT_EQ(arc::GetArcVmDataMigrationStatus(profile_->GetPrefs()),
            arc::ArcVmDataMigrationStatus::kFinished);
  EXPECT_TRUE(
      profile_->GetPrefs()->GetBoolean(arc::prefs::kArcDataRemoveRequested));
  EXPECT_FALSE(screen_->encountered_retriable_fatal_error());
}

}  // namespace
}  // namespace ash
