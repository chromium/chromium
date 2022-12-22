// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/ash/arc/session/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/ui/fake_login_display_host.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/arc/fake_android_management_client.h"
#include "chrome/browser/ash/policy/handlers/powerwash_requirements_checker.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/webui/ash/login/arc_terms_of_service_screen_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/login/auth/auth_metrics_recorder.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_error_factory_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(b/254819616): Replace base::RunLoop().RunUntilIdle() with
// task_environment_.RunUntilIdle() or Run() & Quit() to make the tests less
// fragile.

namespace arc {

namespace {

class ArcInitialStartHandler : public ArcSessionManagerObserver {
 public:
  explicit ArcInitialStartHandler(ArcSessionManager* session_manager)
      : session_manager_(session_manager) {
    session_manager->AddObserver(this);
  }

  ArcInitialStartHandler(const ArcInitialStartHandler&) = delete;
  ArcInitialStartHandler& operator=(const ArcInitialStartHandler&) = delete;

  ~ArcInitialStartHandler() override { session_manager_->RemoveObserver(this); }

  // ArcSessionManagerObserver:
  void OnArcInitialStart() override {
    DCHECK(!was_called_);
    was_called_ = true;
  }

  bool was_called() const { return was_called_; }

 private:
  bool was_called_ = false;

  ArcSessionManager* const session_manager_;
};

class FileExpansionObserver : public ArcSessionManagerObserver {
 public:
  FileExpansionObserver() = default;
  ~FileExpansionObserver() override = default;
  FileExpansionObserver(const FileExpansionObserver&) = delete;
  FileExpansionObserver& operator=(const FileExpansionObserver&) = delete;

  const absl::optional<bool>& property_files_expansion_result() const {
    return property_files_expansion_result_;
  }

  // ArcSessionManagerObserver:
  void OnPropertyFilesExpanded(bool result) override {
    property_files_expansion_result_ = result;
  }

 private:
  absl::optional<bool> property_files_expansion_result_;
};

class ShowErrorObserver : public ArcSessionManagerObserver {
 public:
  ShowErrorObserver(const ShowErrorObserver&) = delete;
  ShowErrorObserver& operator=(const ShowErrorObserver&) = delete;

  explicit ShowErrorObserver(ArcSessionManager* session_manager)
      : session_manager_(session_manager) {
    session_manager->AddObserver(this);
  }

  ~ShowErrorObserver() override { session_manager_->RemoveObserver(this); }

  const absl::optional<ArcSupportHost::ErrorInfo> error_info() const {
    return error_info_;
  }

  void OnArcErrorShowRequested(ArcSupportHost::ErrorInfo error_info) override {
    error_info_ = error_info;
  }

 private:
  absl::optional<ArcSupportHost::ErrorInfo> error_info_;
  ArcSessionManager* const session_manager_;
};

class ArcSessionManagerInLoginScreenTest : public testing::Test {
 public:
  ArcSessionManagerInLoginScreenTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SessionManagerClient::InitializeFakeInMemory();

    ArcSessionManager::SetUiEnabledForTesting(false);
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
  }

  ArcSessionManagerInLoginScreenTest(
      const ArcSessionManagerInLoginScreenTest&) = delete;
  ArcSessionManagerInLoginScreenTest& operator=(
      const ArcSessionManagerInLoginScreenTest&) = delete;

  ~ArcSessionManagerInLoginScreenTest() override {
    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    ash::SessionManagerClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  FakeArcSession* arc_session() {
    return static_cast<FakeArcSession*>(
        arc_session_manager_->GetArcSessionRunnerForTesting()
            ->GetArcSessionForTesting());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
};

// We expect mini instance starts to run if EmitLoginPromptVisible signal is
// emitted.
TEST_F(ArcSessionManagerInLoginScreenTest, EmitLoginPromptVisible) {
  EXPECT_FALSE(arc_session());

  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());

  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  ASSERT_TRUE(arc_session());
  EXPECT_FALSE(arc_session()->is_running());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
}

// We expect mini instance does not start on EmitLoginPromptVisible when ARC
// is not available.
TEST_F(ArcSessionManagerInLoginScreenTest, EmitLoginPromptVisible_NoOp) {
  EXPECT_FALSE(arc_session());

  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  EXPECT_FALSE(arc_session());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
}

// We expect mini instance is not started in manual mode.
TEST_F(ArcSessionManagerInLoginScreenTest, EmitLoginPromptVisibleManualStart) {
  EXPECT_FALSE(arc_session());

  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII("arc-start-mode",
                                                          "manual");

  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  EXPECT_FALSE(arc_session());
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());
}

// We expect that StopMiniArcIfNecessary stops mini-ARC when it is running.
TEST_F(ArcSessionManagerInLoginScreenTest, StopMiniArcIfNecessary) {
  EXPECT_FALSE(arc_session());

  SetArcAvailableCommandLineForTesting(base::CommandLine::ForCurrentProcess());

  ash::SessionManagerClient::Get()->EmitLoginPromptVisible();
  EXPECT_TRUE(arc_session());

  arc_session_manager()->StopMiniArcIfNecessary();
  EXPECT_FALSE(arc_session());
}

class ArcSessionManagerTestBase : public testing::Test {
 public:
  ArcSessionManagerTestBase()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {
    TestingBrowserProcess::GetGlobal()->SetLocalState(&test_local_state_);
    arc::prefs::RegisterLocalStatePrefs(test_local_state_.registry());
    ash::DemoSetupController::RegisterLocalStatePrefs(
        test_local_state_.registry());
    ash::device_settings_cache::RegisterPrefs(test_local_state_.registry());
    user_manager::KnownUser::RegisterPrefs(test_local_state_.registry());
    auth_metrics_recorder_ = ash::AuthMetricsRecorder::CreateForTesting();
  }

  ArcSessionManagerTestBase(const ArcSessionManagerTestBase&) = delete;
  ArcSessionManagerTestBase& operator=(const ArcSessionManagerTestBase&) =
      delete;

  ~ArcSessionManagerTestBase() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    chromeos::PowerManagerClient::InitializeFake();
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::UpstartClient::InitializeFake();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@example.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    StartPreferenceSyncing();

    ASSERT_FALSE(arc_session_manager_->enable_requested());
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    ash::UpstartClient::Shutdown();
    ash::SessionManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  TestingProfile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  bool WaitForDataRemoved(ArcSessionManager::State expected_state) {
    if (arc_session_manager()->state() !=
        ArcSessionManager::State::REMOVING_DATA_DIR)
      return false;

    base::RunLoop().RunUntilIdle();
    if (arc_session_manager()->state() != expected_state)
      return false;

    return true;
  }

 private:
  void StartPreferenceSyncing() const {
    PrefServiceSyncableFromProfile(profile_.get())
        ->GetSyncableService(syncer::PREFERENCES)
        ->MergeDataAndStartSyncing(
            syncer::PREFERENCES, syncer::SyncDataList(),
            std::make_unique<syncer::FakeSyncChangeProcessor>(),
            std::make_unique<syncer::SyncErrorFactoryMock>());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple test_local_state_;
  std::unique_ptr<ash::AuthMetricsRecorder> auth_metrics_recorder_;
};

class ArcSessionManagerTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerTest() = default;

  ArcSessionManagerTest(const ArcSessionManagerTest&) = delete;
  ArcSessionManagerTest& operator=(const ArcSessionManagerTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    ash::CryptohomeMiscClient::InitializeFake();
    ash::FakeCryptohomeMiscClient::Get()->set_requires_powerwash(false);
    policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();

    ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
              arc_session_manager()->state());
  }

  void TearDown() override {
    ash::CryptohomeMiscClient::Shutdown();
    ArcSessionManagerTestBase::TearDown();
  }
};

TEST_F(ArcSessionManagerTest, BaseWorkflow) {
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());

  const base::TimeTicks enabled_time = base::TimeTicks::Now();

  // Enables ARC. First time, ToS negotiation should start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  const base::TimeTicks after_enabled_time = base::TimeTicks::Now();

  const base::TimeTicks pre_start_time =
      arc_session_manager()->pre_start_time();
  EXPECT_FALSE(pre_start_time.is_null());
  EXPECT_GE(pre_start_time, enabled_time);
  EXPECT_GE(after_enabled_time, pre_start_time);
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());

  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();

  const base::TimeTicks start_time = arc_session_manager()->start_time();
  EXPECT_FALSE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_EQ(pre_start_time, arc_session_manager()->pre_start_time());
  EXPECT_FALSE(start_time.is_null());
  EXPECT_GE(start_time, after_enabled_time);
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();

  EXPECT_TRUE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());
}

TEST_F(ArcSessionManagerTest, SignedInWorkflow) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When signed-in, enabling ARC results in the ACTIVE state.
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
}

TEST_F(ArcSessionManagerTest, SignedInWorkflowWithArcOnDemand) {
  base::HistogramTester histogram_tester;

  // Enable ARC on Demand feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kArcOnDemandFeature);
  // ARC on Demand is enabled only for managed users.
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  // ARC on Demand is enabled only on ARCVM.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);
  prefs->SetBoolean(prefs::kArcPackagesIsUpToDate, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When signed-in, enabling ARC results in the READY state.
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::READY, arc_session_manager()->state());
  histogram_tester.ExpectUniqueSample(
      "Arc.DelayedActivation.ActivationIsDelayed", true, 1);

  constexpr auto kDelay = base::Minutes(10);
  task_environment().FastForwardBy(kDelay);

  // ARC starts after calling AllowActivation().
  arc_session_manager()->AllowActivation();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  histogram_tester.ExpectUniqueTimeSample("Arc.DelayedActivation.Delay", kDelay,
                                          1);
}

TEST_F(ArcSessionManagerTest, SignedInWorkflow_ActivationIsAlreadyAllowed) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When signed-in, enabling ARC results in the ACTIVE state if
  // AllowActivation() is called beforehand.
  arc_session_manager()->AllowActivation();
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
}

// Tests that tying to enable ARC++ with an incompatible file system fails and
// shows the user a notification to that effect.
TEST_F(ArcSessionManagerTest, MigrationGuideNotification) {
  ArcSessionManager::SetUiEnabledForTesting(true);
  ArcSessionManager::EnableCheckAndroidManagementForTesting(false);
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  NotificationDisplayServiceTester notification_service(profile());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());
  auto notifications = notification_service.GetDisplayedNotificationsForType(
      NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(1U, notifications.size());
  EXPECT_EQ("arc_fs_migration/suggest", notifications[0].id());
}

// Tests that OnArcInitialStart is called  after the successful ARC provisioning
// on the first start after OptIn.
TEST_F(ArcSessionManagerTest, ArcInitialStartFirstProvisioning) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcInitialStartHandler start_handler(arc_session_manager());
  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();

  EXPECT_FALSE(start_handler.was_called());

  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_TRUE(start_handler.was_called());

  arc_session_manager()->Shutdown();
}

// Tests that OnArcInitialStart is not called after the successful ARC
// provisioning on the second and next starts after OptIn.
TEST_F(ArcSessionManagerTest, ArcInitialStartNextProvisioning) {
  // Set up the situation that provisioning is successfully done in the
  // previous session. In this case initial start callback is not called.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcInitialStartHandler start_handler(arc_session_manager());

  arc_session_manager()->RequestEnable();
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_FALSE(start_handler.was_called());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksTermsOfService) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC first time. ToS negotiation should NOT happen.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksArcStart) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC second time. ARC should NOT start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CancelFetchingDisablesArc) {
  SetArcPlayStoreEnabledForProfile(profile(), true);

  // Starts ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  // Emulate to cancel the ToS UI (e.g. closing the window).
  arc_session_manager()->CancelAuthCode();

  // Google Play Store enabled preference should be set to false, too.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Emulate the preference handling.
  arc_session_manager()->RequestDisableWithArcDataRemoval();

  // Wait until data is removed.
  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CloseUIKeepsArcEnabled) {
  // Starts ARC.
  SetArcPlayStoreEnabledForProfile(profile(), true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // When ARC is properly started, closing UI should be no-op.
  arc_session_manager()->CancelAuthCode();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, Provisioning_Success) {
  PrefService* const prefs = profile()->GetPrefs();

  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  ASSERT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  // Emulate to accept the terms of service.
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_FALSE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Here, provisining is not yet completed, so kArcSignedIn should be false.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Emulate successful provisioning.
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());
}

// Verifies that Play Store shown is suppressed on restart when required.
TEST_F(ArcSessionManagerTest, PlayStoreSuppressed) {
  // Set up the situation that terms were accepted in the previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  // Set the flag indicating that the provisioning was initiated from OOBE in
  // the previous session.
  prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  arc_session_manager()->StartArcForTesting();

  // Second start, no fetching code is expected.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  // Completing the provisioning resets this flag.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  // |prefs::kArcProvisioningInitiatedFromOobe| flag prevents opening the
  // Play Store.
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, InitiatedFromOobeIsResetOnOptOut) {
  // Set up the situation that terms were accepted in the previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  // Set the flag indicating that the provisioning was initiated from OOBE in
  // the previous session.
  prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));
  // Disabling ARC resets suppress state
  arc_session_manager()->RequestDisable();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, Provisioning_Restart) {
  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->AllowActivation();
  arc_session_manager()->RequestEnable();

  // Second start, no fetching code is expected.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report failure.
  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewSignInError(
          arc::mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR));
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  // On error, UI to send feedback is showing. In that case,
  // the ARC is still necessary to run on background for gathering the logs.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RemoveDataDir) {
  // Emulate the situation where the initial Google Play Store enabled
  // preference is false for managed user, i.e., data dir is being removed at
  // beginning.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestArcDataRemoval();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());

  // Enable ARC. Data is removed asyncronously. At this moment session manager
  // should be in REMOVING_DATA_DIR state.
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  // Wait until data is removed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Request to remove data and stop session manager.
  arc_session_manager()->RequestArcDataRemoval();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->Shutdown();
  base::RunLoop().RunUntilIdle();
  // Request should persist.
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
}

TEST_F(ArcSessionManagerTest, RemoveDataDir_Restart) {
  // Emulate second sign-in. Data should be removed first and ARC started after.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcDataRemoveRequested, true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  ASSERT_TRUE(
      WaitForDataRemoved(ArcSessionManager::State::CHECKING_REQUIREMENTS));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RegularToChildTransition) {
  // Emulate the situation where a regular user has transitioned to a child
  // account.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::REGULAR_TO_CHILD));
  base::test::ScopedFeatureList feature_list;

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(static_cast<int>(ArcManagementTransition::REGULAR_TO_CHILD),
            profile()->GetPrefs()->GetInteger(prefs::kArcManagementTransition));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, ClearArcTransitionOnShutdown) {
  profile()->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::NO_TRANSITION));

  // Initialize ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_EQ(static_cast<int>(ArcManagementTransition::NO_TRANSITION),
            profile()->GetPrefs()->GetInteger(prefs::kArcManagementTransition));

  // Child started graduation.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::CHILD_TO_REGULAR));
  // Simulate ARC shutdown.
  arc_session_manager()->RequestDisableWithArcDataRemoval();
  EXPECT_EQ(static_cast<int>(ArcManagementTransition::NO_TRANSITION),
            profile()->GetPrefs()->GetInteger(prefs::kArcManagementTransition));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, ClearArcTransitionOnArcDataRemoval) {
  EXPECT_EQ(ArcManagementTransition::NO_TRANSITION,
            arc::GetManagementTransition(profile()));

  // Initialize ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_EQ(ArcManagementTransition::NO_TRANSITION,
            arc::GetManagementTransition(profile()));

  // Child started graduation.
  profile()->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      static_cast<int>(ArcManagementTransition::CHILD_TO_REGULAR));

  arc_session_manager()->RequestArcDataRemoval();
  EXPECT_EQ(ArcManagementTransition::NO_TRANSITION,
            arc::GetManagementTransition(profile()));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IgnoreSecondErrorReporting) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report some failure that does not stop the bridge.
  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewSignInError(
          arc::mojom::GMSSignInError::GMS_SIGN_IN_FAILED));
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Try to send another error that stops the bridge if sent first. It should
  // be ignored.
  result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Test case when skipped ToS flag is not set during the ARC boot.
TEST_F(ArcSessionManagerTest, SkippedTermsOfServiceNegotiationFalse) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // On initial start skipped ToS flag is not set.
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  arc_session_manager()->Shutdown();
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
}

// Test case when skipped ToS flag is set during the ARC boot.
// Preconditions are: ToS accepted and ARC was signed in.
TEST_F(ArcSessionManagerTest, SkippedTermsOfServiceNegotiationTrue) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  arc_session_manager()->AllowActivation();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_manager()->skipped_terms_of_service_negotiation());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Disabling ARC turns skipped ToS flag off.
  arc_session_manager()->RequestDisable();
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  arc_session_manager()->Shutdown();
}

// Test case when skipped ToS flag is preserved during the internal ARC restart.
TEST_F(ArcSessionManagerTest,
       SkippedTermsOfServiceNegotiationOnInternalRestart) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->AllowActivation();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());

  // Simualate internal restart.
  arc_session_manager()->StopAndEnableArc();
  // Fake ARC session implementation synchronously calls stop callback and
  // session manager should be reactivated at this moment.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  // Skipped ToS flag should be preserved.
  EXPECT_FALSE(arc_session_manager()->skipped_terms_of_service_negotiation());
  arc_session_manager()->Shutdown();
}

// In case of the next start ArcSessionManager should go through remove data
// folder phase before negotiating terms of service.
TEST_F(ArcSessionManagerTest, DataCleanUpOnFirstStart) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kArcDataCleanupOnStart);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcPlayStoreEnabledPreferenceHandler handler(profile(),
                                               arc_session_manager());
  handler.Start();

  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// In case of the next start ArcSessionManager should go through remove data
// folder phase before activating.
TEST_F(ArcSessionManagerTest, DataCleanUpOnNextStart) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kArcDataCleanupOnStart);

  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);
  prefs->SetBoolean(prefs::kArcEnabled, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  ArcPlayStoreEnabledPreferenceHandler handler(profile(),
                                               arc_session_manager());
  handler.Start();

  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RequestDisableDoesNotRemoveData) {
  // Start ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  // Disable ARC.
  arc_session_manager()->RequestDisable();

  // Data removal is not requested.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RequestDisableWithArcDataRemoval) {
  // Start ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());

  // Disable ARC and remove ARC data.
  arc_session_manager()->RequestDisableWithArcDataRemoval();

  // Data removal is requested.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

// Tests that |vm_info| is initialized with absl::nullopt.
TEST_F(ArcSessionManagerTest, GetVmInfo_InitialValue) {
  const auto& vm_info = arc_session_manager()->GetVmInfo();
  EXPECT_EQ(absl::nullopt, vm_info);
}

// Tests that |vm_info| is updated with that from VmStartedSignal.
TEST_F(ArcSessionManagerTest, GetVmInfo_WithVmStarted) {
  vm_tools::concierge::VmStartedSignal vm_signal;
  vm_signal.set_name(kArcVmName);
  vm_signal.mutable_vm_info()->set_seneschal_server_handle(1000UL);
  arc_session_manager()->OnVmStarted(vm_signal);

  const auto& vm_info = arc_session_manager()->GetVmInfo();
  ASSERT_NE(absl::nullopt, vm_info);
  EXPECT_EQ(1000UL, vm_info->seneschal_server_handle());
}

// Tests that |vm_info| remains as absl::nullopt after VM stops.
TEST_F(ArcSessionManagerTest, GetVmInfo_WithVmStopped) {
  vm_tools::concierge::VmStoppedSignal vm_signal;
  vm_signal.set_name(kArcVmName);
  arc_session_manager()->OnVmStopped(vm_signal);

  const auto& vm_info = arc_session_manager()->GetVmInfo();
  EXPECT_EQ(absl::nullopt, vm_info);
}

// Tests that |vm_info| is reset to absl::nullopt after VM starts and stops.
TEST_F(ArcSessionManagerTest, GetVmInfo_WithVmStarted_ThenStopped) {
  vm_tools::concierge::VmStartedSignal start_signal;
  start_signal.set_name(kArcVmName);
  start_signal.mutable_vm_info()->set_seneschal_server_handle(1000UL);
  arc_session_manager()->OnVmStarted(start_signal);

  vm_tools::concierge::VmStoppedSignal stop_signal;
  stop_signal.set_name(kArcVmName);
  arc_session_manager()->OnVmStopped(stop_signal);

  const auto& vm_info = arc_session_manager()->GetVmInfo();
  EXPECT_EQ(absl::nullopt, vm_info);
}

// Tests that |vm_info| is not updated with non-ARCVM VmStartedSignal.
TEST_F(ArcSessionManagerTest, GetVmInfo_WithNonVmStarted) {
  vm_tools::concierge::VmStartedSignal non_vm_signal;
  non_vm_signal.set_name("non-ARCVM");
  non_vm_signal.mutable_vm_info()->set_seneschal_server_handle(1000UL);
  arc_session_manager()->OnVmStarted(non_vm_signal);

  const auto& vm_info = arc_session_manager()->GetVmInfo();
  EXPECT_EQ(absl::nullopt, vm_info);
}

class ArcSessionManagerArcAlwaysStartTest : public ArcSessionManagerTest {
 public:
  ArcSessionManagerArcAlwaysStartTest() = default;

  ArcSessionManagerArcAlwaysStartTest(
      const ArcSessionManagerArcAlwaysStartTest&) = delete;
  ArcSessionManagerArcAlwaysStartTest& operator=(
      const ArcSessionManagerArcAlwaysStartTest&) = delete;

  void SetUp() override {
    SetArcAlwaysStartWithoutPlayStoreForTesting();
    ArcSessionManagerTest::SetUp();
  }
};

ArcProvisioningResult CreateProvisioningResult(
    const absl::variant<arc::mojom::GeneralSignInError,
                        arc::mojom::GMSSignInError,
                        arc::mojom::GMSCheckInError,
                        arc::mojom::CloudProvisionFlowError,
                        ArcStopReason,
                        ChromeProvisioningTimeout>& error) {
  if (absl::holds_alternative<arc::mojom::GeneralSignInError>(error)) {
    return ArcProvisioningResult(arc::mojom::ArcSignInResult::NewError(
        arc::mojom::ArcSignInError::NewGeneralError(
            absl::get<arc::mojom::GeneralSignInError>(error))));
  }

  if (absl::holds_alternative<arc::mojom::GMSSignInError>(error)) {
    return ArcProvisioningResult(arc::mojom::ArcSignInResult::NewError(
        arc::mojom::ArcSignInError::NewSignInError(
            absl::get<arc::mojom::GMSSignInError>(error))));
  }

  if (absl::holds_alternative<arc::mojom::GMSCheckInError>(error)) {
    return ArcProvisioningResult(arc::mojom::ArcSignInResult::NewError(
        arc::mojom::ArcSignInError::NewCheckInError(
            absl::get<arc::mojom::GMSCheckInError>(error))));
  }

  if (absl::holds_alternative<arc::mojom::CloudProvisionFlowError>(error)) {
    return ArcProvisioningResult(arc::mojom::ArcSignInResult::NewError(
        arc::mojom::ArcSignInError::NewCloudProvisionFlowError(
            absl::get<arc::mojom::CloudProvisionFlowError>(error))));
  }

  if (absl::holds_alternative<ArcStopReason>(error))
    return ArcProvisioningResult(absl::get<ArcStopReason>(error));

  return ArcProvisioningResult(ChromeProvisioningTimeout{});
}

struct ProvisioningErrorDisplayTestParam {
  // the reason for arc instance stopping
  absl::variant<arc::mojom::GeneralSignInError,
                arc::mojom::GMSSignInError,
                arc::mojom::GMSCheckInError,
                arc::mojom::CloudProvisionFlowError,
                ArcStopReason,
                ChromeProvisioningTimeout>
      error;

  // the error sent to arc support host
  ArcSupportHost::Error message;

  // the error code sent to arc support host
  absl::optional<int> arg;
};

constexpr ProvisioningErrorDisplayTestParam
    kProvisioningErrorDisplayTestCases[] = {
        {ArcStopReason::GENERIC_BOOT_FAILURE,
         ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR, 8 /*ARC_STOPPED*/},
        {ArcStopReason::LOW_DISK_SPACE,
         ArcSupportHost::Error::LOW_DISK_SPACE_ERROR,
         {}},
        {ArcStopReason::CRASH, ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR,
         8 /*ARC_STOPPED*/},
        {arc::mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR,
         ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR,
         1 /*GMS_SIGN_IN_NETWORK_ERROR*/},
        {arc::mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT,
         ArcSupportHost::Error::SIGN_IN_GMS_SIGNIN_ERROR,
         5 /*GMS_SIGN_IN_TIMEOUT*/},
        {arc::mojom::GMSCheckInError::GMS_CHECK_IN_TIMEOUT,
         ArcSupportHost::Error::SIGN_IN_GMS_CHECKIN_ERROR,
         2 /*GMS_CHECK_IN_TIMEOUT*/}};

class ProvisioningErrorDisplayTest
    : public ArcSessionManagerTest,
      public testing::WithParamInterface<ProvisioningErrorDisplayTestParam> {
  void SetUp() override {
    ArcSessionManagerTest::SetUp();

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();
    arc_session_manager()->RequestEnable();
  }

  void TearDown() override {
    arc_session_manager()->Shutdown();
    ArcSessionManagerTest::TearDown();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ProvisioningErrorDisplayTest,
    ::testing::ValuesIn(kProvisioningErrorDisplayTestCases));

TEST_P(ProvisioningErrorDisplayTest, ArcStopped) {
  ShowErrorObserver observer(arc_session_manager());

  ArcProvisioningResult result = CreateProvisioningResult(GetParam().error);
  arc_session_manager()->OnProvisioningFinished(result);

  ASSERT_TRUE(observer.error_info());
  EXPECT_EQ(GetParam().message, observer.error_info().value().error);
  EXPECT_EQ(GetParam().arg, observer.error_info().value().arg);
}

TEST_F(ArcSessionManagerArcAlwaysStartTest, BaseWorkflow) {
  // TODO(victorhsieh): Consider also tracking sign-in activity, which is
  // initiated from the Android side.
  EXPECT_TRUE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When ARC is always started, ArcSessionManager should always be in ACTIVE
  // state.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->pre_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->start_time().is_null());

  arc_session_manager()->Shutdown();
}

class ArcSessionManagerPolicyTest
    : public ArcSessionManagerTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, int, int>> {
 public:
  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    AccountId account_id;
    if (is_active_directory_user()) {
      account_id = AccountId(AccountId::AdFromUserEmailObjGuid(
          profile()->GetProfileUserName(), "1234567890"));
    } else {
      account_id = AccountId(AccountId::FromUserEmailGaiaId(
          profile()->GetProfileUserName(), "1234567890"));
    }
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    // Mocks OOBE environment so that IsArcOobeOptInActive() returns true.
    if (is_oobe_optin()) {
      CreateLoginDisplayHost();
    }
  }

  void TearDown() override {
    if (is_oobe_optin()) {
      fake_login_display_host_.reset();
      TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    }
    ArcSessionManagerTestBase::TearDown();
  }

  bool arc_enabled_pref_managed() const { return std::get<0>(GetParam()); }

  bool is_active_directory_user() const { return std::get<1>(GetParam()); }

  bool is_oobe_optin() const { return std::get<2>(GetParam()); }

  base::Value backup_restore_pref_value() const {
    switch (std::get<3>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }

  base::Value location_service_pref_value() const {
    switch (std::get<4>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }

 private:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
};

TEST_P(ArcSessionManagerPolicyTest, SkippingTerms) {
  sync_preferences::TestingPrefServiceSyncable* const prefs =
      profile()->GetTestingPrefService();

  // Backup-restore and location-service prefs are off by default.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcTermsAccepted));

  EXPECT_EQ(is_active_directory_user(),
            IsActiveDirectoryUserForProfile(profile()));

  // Enable ARC through user pref or by policy, according to the test parameter.
  if (arc_enabled_pref_managed())
    prefs->SetManagedPref(prefs::kArcEnabled,
                          std::make_unique<base::Value>(true));
  else
    prefs->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Assign test values to the prefs.
  if (backup_restore_pref_value().is_bool()) {
    prefs->SetManagedPref(
        prefs::kArcBackupRestoreEnabled,
        base::Value::ToUniquePtrValue(backup_restore_pref_value().Clone()));
  }
  if (location_service_pref_value().is_bool()) {
    prefs->SetManagedPref(
        prefs::kArcLocationServiceEnabled,
        base::Value::ToUniquePtrValue(location_service_pref_value().Clone()));
  }

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  // Terms of Service are skipped if ARC is enabled by policy and both policies
  // are either managed or unused (for Active Directory users a LaForge
  // account is created, not a full Dasher account, where the policies have no
  // meaning).
  // Terms of Service are skipped if ARC is enabled by policy and if it's in
  // session opt-in.
  const bool prefs_unused = is_active_directory_user();
  const bool backup_managed = backup_restore_pref_value().is_bool();
  const bool location_managed = location_service_pref_value().is_bool();
  const bool is_arc_oobe_optin = is_oobe_optin();
  const bool expected_terms_skipping =
      arc_enabled_pref_managed() && ((backup_managed && location_managed) ||
                                     prefs_unused || !is_arc_oobe_optin);
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  EXPECT_EQ(IsArcOobeOptInActive(), is_arc_oobe_optin);

  // Complete provisioning if it's not done yet.
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();

  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));

  // Play Store app is launched unless the Terms screen was suppressed or Tos is
  // accepted during OOBE.
  EXPECT_NE(expected_terms_skipping || is_arc_oobe_optin,
            arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // In case Tos is skipped, B&R and GLS should not be set if not managed.
  if (expected_terms_skipping) {
    if (!backup_managed)
      EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
    if (!location_managed)
      EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));
  }

  // Managed values for the prefs are unset.
  prefs->RemoveManagedPref(prefs::kArcBackupRestoreEnabled);
  prefs->RemoveManagedPref(prefs::kArcLocationServiceEnabled);

  // The ARC state is preserved. The prefs return to the default false values.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Stop ARC and shutdown the service.
  prefs->RemoveManagedPref(prefs::kArcEnabled);
  WaitForDataRemoved(ArcSessionManager::State::STOPPED);
  arc_session_manager()->Shutdown();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcSessionManagerPolicyTest,
    // testing::Values is incompatible with move-only types, hence ints are used
    // as a proxy for base::Value.
    testing::Combine(testing::Bool() /* arc_enabled_pref_managed */,
                     testing::Bool() /* is_active_directory_user */,
                     testing::Bool() /* is_oobe_optin */,
                     /* backup_restore_pref_value */
                     testing::Values(0,   // base::Value()
                                     1,   // base::Value(false)
                                     2),  // base::Value(true)
                     /* location_service_pref_value */
                     testing::Values(0,     // base::Value()
                                     1,     // base::Value(false)
                                     2)));  // base::Value(true)

class ArcSessionManagerKioskTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerKioskTest() = default;

  ArcSessionManagerKioskTest(const ArcSessionManagerKioskTest&) = delete;
  ArcSessionManagerKioskTest& operator=(const ArcSessionManagerKioskTest&) =
      delete;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }
};

TEST_F(ArcSessionManagerKioskTest, AuthFailure) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is invoked exactly once in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::BindRepeating([](bool* terminated) { *terminated = true; },
                          &terminated));

  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_TRUE(terminated);
}

class ArcSessionManagerPublicSessionTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerPublicSessionTest() = default;

  ArcSessionManagerPublicSessionTest(
      const ArcSessionManagerPublicSessionTest&) = delete;
  ArcSessionManagerPublicSessionTest& operator=(
      const ArcSessionManagerPublicSessionTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddPublicAccountUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }
};

TEST_F(ArcSessionManagerPublicSessionTest, AuthFailure) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is never invoked in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::BindRepeating([](bool* terminated) { *terminated = true; },
                          &terminated));

  arc::mojom::ArcSignInResultPtr result = arc::mojom::ArcSignInResult::NewError(
      arc::mojom::ArcSignInError::NewGeneralError(
          arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR));
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_FALSE(terminated);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
}

class ArcSessionOobeOptInNegotiatorTest
    : public ArcSessionManagerTest,
      public ash::ArcTermsOfServiceScreenView,
      public testing::WithParamInterface<bool> {
 public:
  ArcSessionOobeOptInNegotiatorTest() {
    // This test only works with the ARC ToS screen, which would be replaced
    // by the Consolidated Consent screen when the feature
    // OobeConsolidatedConsent is enabled. Make sure that the
    // OobeConsolidatedConsent feature is disabled before running these tests.
    // TODO(crbug,com/1297250): Implement similar tests to test the interaction
    // between the ArcSessionOobeOptInNegotiatorTest and the Consolidated
    // Consent screen.
    feature_list_.InitAndDisableFeature(
        ash::features::kOobeConsolidatedConsent);
  }

  ArcSessionOobeOptInNegotiatorTest(const ArcSessionOobeOptInNegotiatorTest&) =
      delete;
  ArcSessionOobeOptInNegotiatorTest& operator=(
      const ArcSessionOobeOptInNegotiatorTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTest::SetUp();

    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        true);
    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        this);

    CreateLoginDisplayHost();

    if (IsManagedUser()) {
      policy::ProfilePolicyConnector* const connector =
          profile()->GetProfilePolicyConnector();
      connector->OverrideIsManagedForTesting(true);

      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcEnabled, std::make_unique<base::Value>(true));
    }

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    if (IsArcPlayStoreEnabledForProfile(profile()))
      arc_session_manager()->RequestEnable();
  }

  void TearDown() override {
    // Correctly stop service.
    arc_session_manager()->Shutdown();

    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        nullptr);
    ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
        false);
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    ArcSessionManagerTest::TearDown();
  }

 protected:
  bool IsManagedUser() { return GetParam(); }

  void ReportAccepted() {
    for (auto& observer : observer_list_) {
      observer.OnAccept(false);
    }
    base::RunLoop().RunUntilIdle();
  }

  void ReportViewDestroyed() {
    for (auto& observer : observer_list_)
      observer.OnViewDestroyed(this);
    base::RunLoop().RunUntilIdle();
  }

  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<ash::FakeLoginDisplayHost>();
  }

  ash::FakeLoginDisplayHost* login_display_host() {
    return fake_login_display_host_.get();
  }

  void CloseLoginDisplayHost() { fake_login_display_host_.reset(); }

  ash::ArcTermsOfServiceScreenView* view() { return this; }

 private:
  // ArcTermsOfServiceScreenView:
  void AddObserver(
      ash::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(
      ash::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Show() override {
    // To match ArcTermsOfServiceScreenHandler logic where Google Play Store
    // enabled preferencee is set to true on showing UI, which eventually
    // triggers to call RequestEnable().
    arc_session_manager()->RequestEnable();
  }

  void Hide() override {}

  base::ObserverList<ash::ArcTermsOfServiceScreenViewObserver>::Unchecked
      observer_list_;
  std::unique_ptr<ash::FakeLoginDisplayHost> fake_login_display_host_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionOobeOptInNegotiatorTest,
                         ::testing::Values(true, false));

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsAccepted) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  ReportAccepted();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
}

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsViewDestroyed) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  CloseLoginDisplayHost();
  ReportViewDestroyed();
  if (!IsManagedUser()) {
    // ArcPlayStoreEnabledPreferenceHandler is not running, so the state should
    // be kept as is.
    EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
              arc_session_manager()->state());
    EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  } else {
    // For managed case we handle closing outside of
    // ArcPlayStoreEnabledPreferenceHandler. So it session turns to STOPPED.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
    // Managed user's preference should not be overwritten.
    EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  }
}

struct ArcSessionRetryTestParam {
  enum class Negotiation {
    // Negotiation is required for provisioning.
    REQUIRED,
    // Negotiation is not required and not shown for provisioning.
    SKIPPED,
  };

  Negotiation negotiation;

  // Whether ARC++ container is alive on error.
  bool container_alive;

  // Whether data is removed on error.
  bool data_removed;

  absl::variant<arc::mojom::GeneralSignInError,
                arc::mojom::GMSSignInError,
                arc::mojom::GMSCheckInError,
                arc::mojom::CloudProvisionFlowError,
                ArcStopReason,
                ChromeProvisioningTimeout>
      error;
};

ArcSessionRetryTestParam kRetryTestCases[] = {
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, true,
     arc::mojom::GeneralSignInError::UNKNOWN_ERROR},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_SERVICE_UNAVAILABLE},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_BAD_AUTHENTICATION},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSCheckInError::GMS_CHECK_IN_FAILED},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED, true, true,
     arc::mojom::CloudProvisionFlowError::ERROR_OTHER},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GeneralSignInError::MOJO_VERSION_MISMATCH},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GeneralSignInError::GENERIC_PROVISIONING_TIMEOUT},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSCheckInError::GMS_CHECK_IN_TIMEOUT},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSCheckInError::GMS_CHECK_IN_INTERNAL_ERROR},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_FAILED},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GMSSignInError::GMS_SIGN_IN_INTERNAL_ERROR},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED, true, true,
     arc::mojom::CloudProvisionFlowError::ERROR_TIMEOUT},
    {ArcSessionRetryTestParam::Negotiation::SKIPPED, true, true,
     arc::mojom::CloudProvisionFlowError::ERROR_JSON},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, false, false,
     ArcStopReason::CRASH},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, true,
     ChromeProvisioningTimeout{}},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR},
    {ArcSessionRetryTestParam::Negotiation::REQUIRED, true, false,
     arc::mojom::GeneralSignInError::NO_NETWORK_CONNECTION},
};

class ArcSessionRetryTest
    : public ArcSessionManagerTest,
      public testing::WithParamInterface<ArcSessionRetryTestParam> {
 public:
  ArcSessionRetryTest() = default;

  ArcSessionRetryTest(const ArcSessionRetryTest&) = delete;
  ArcSessionRetryTest& operator=(const ArcSessionRetryTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTest::SetUp();

    GetFakeUserManager()->set_current_user_new(true);

    // Make negotiation not needed by switching to managed flow with other
    // preferences under the policy, similar to google.com provisioning case.
    if (GetParam().negotiation ==
        ArcSessionRetryTestParam::Negotiation::SKIPPED) {
      policy::ProfilePolicyConnector* const connector =
          profile()->GetProfilePolicyConnector();
      connector->OverrideIsManagedForTesting(true);

      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcEnabled, std::make_unique<base::Value>(true));
      // Set all prefs as managed to simulate google.com account provisioning.
      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcBackupRestoreEnabled,
          std::make_unique<base::Value>(false));
      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcLocationServiceEnabled,
          std::make_unique<base::Value>(false));
      EXPECT_FALSE(arc::IsArcTermsOfServiceNegotiationNeeded(profile()));
    }
  }

  void TearDown() override {
    // Correctly stop service.
    arc_session_manager()->Shutdown();
    ArcSessionManagerTest::TearDown();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         ArcSessionRetryTest,
                         ::testing::ValuesIn(kRetryTestCases));

// Verifies that Android container behaves as expected.* This checks:
//   * Whether ARC++ container alive or not on error.
//   * Whether Android data is removed or not on error.
//   * ARC++ Container is restared on retry.
TEST_P(ArcSessionRetryTest, ContainerRestarted) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  EXPECT_EQ(ArcSessionManager::State::CHECKING_REQUIREMENTS,
            arc_session_manager()->state());
  arc_session_manager()->EmulateRequirementCheckCompletionForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  ArcProvisioningResult result1 = CreateProvisioningResult(GetParam().error);
  arc_session_manager()->OnProvisioningFinished(result1);

  // In case of permanent error data removal request is scheduled.
  EXPECT_EQ(GetParam().data_removed,
            profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  if (GetParam().container_alive) {
    // We don't stop ARC due to let user submit user feedback with alive Android
    // container.
    EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  } else {
    // Container is stopped automatically on this error.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
  }

  arc_session_manager()->OnRetryClicked();

  if (GetParam().data_removed) {
    // Check state goes from REMOVING_DATA_DIR to CHECKING_REQUIREMENTS
    EXPECT_TRUE(
        WaitForDataRemoved(ArcSessionManager::State::CHECKING_REQUIREMENTS));
  }

  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Successful retry keeps ARC++ container running.
  arc::mojom::ArcSignInResultPtr result =
      arc::mojom::ArcSignInResult::NewSuccess(
          arc::mojom::ArcSignInSuccess::SUCCESS);
  arc_session_manager()->OnProvisioningFinished(
      ArcProvisioningResult(std::move(result)));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Test that when files have already been expaneded, AddObserver() immediately
// calls OnPropertyFilesExpanded().
TEST_F(ArcSessionManagerTest, FileExpansion_AlreadyDone) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->OnExpandPropertyFilesAndReadSaltForTesting(true);
  arc_session_manager()->AddObserver(&observer);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_TRUE(observer.property_files_expansion_result().value());
}

// Tests that OnPropertyFilesExpanded() is called with true when the files are
// expended.
TEST_F(ArcSessionManagerTest, FileExpansion) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->AddObserver(&observer);
  EXPECT_FALSE(observer.property_files_expansion_result().has_value());
  arc_session_manager()->OnExpandPropertyFilesAndReadSaltForTesting(true);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_TRUE(observer.property_files_expansion_result().value());
}

// Tests that OnPropertyFilesExpanded() is called with false when the expansion
// failed.
TEST_F(ArcSessionManagerTest, FileExpansion_Fail) {
  arc_session_manager()->reset_property_files_expansion_result();
  FileExpansionObserver observer;
  arc_session_manager()->AddObserver(&observer);
  EXPECT_FALSE(observer.property_files_expansion_result().has_value());
  arc_session_manager()->OnExpandPropertyFilesAndReadSaltForTesting(false);
  ASSERT_TRUE(observer.property_files_expansion_result().has_value());
  EXPECT_FALSE(observer.property_files_expansion_result().value());
}

// Tests that TrimVmMemory doesn't crash.
TEST_F(ArcSessionManagerTest, TrimVmMemory) {
  bool callback_called = false;
  arc_session_manager()->TrimVmMemory(
      base::BindLambdaForTesting([&callback_called](bool, const std::string&) {
        callback_called = true;
      }),
      0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

class ArcSessionManagerPowerwashTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerPowerwashTest() = default;
  ~ArcSessionManagerPowerwashTest() override = default;
  ArcSessionManagerPowerwashTest(const ArcSessionManagerPowerwashTest&) =
      delete;
  ArcSessionManagerPowerwashTest& operator=(
      const ArcSessionManagerPowerwashTest&) = delete;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    ash::CryptohomeMiscClient::InitializeFake();
  }

  void TearDown() override {
    ash::CryptohomeMiscClient::Shutdown();
    ArcSessionManagerTestBase::TearDown();
  }
};

TEST_F(ArcSessionManagerPowerwashTest, PowerwashRequestBlocksArcStart) {
  EXPECT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
            arc_session_manager()->state());

  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  // Login unaffiliated user.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile()->GetProfileUserName(), "1234567890"));
  GetFakeUserManager()->AddUserWithAffiliation(account_id, false);
  GetFakeUserManager()->LoginUser(account_id);

  // Set DeviceRebootOnUserSignout to ALWAYS.
  ash::ScopedCrosSettingsTestHelper settings_helper{
      /* create_settings_service=*/false};
  settings_helper.ReplaceDeviceSettingsProviderWithStub();
  settings_helper.SetInteger(
      ash::kDeviceRebootOnUserSignout,
      enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);

  // Initialize cryptohome to require powerwash.
  ash::FakeCryptohomeMiscClient::Get()->set_requires_powerwash(true);
  policy::PowerwashRequirementsChecker::InitializeSynchronouslyForTesting();

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  arc_session_manager()->RequestEnable();
  // Wait for manager's state.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

class ArcTransitionToManagedTest
    : public ArcSessionManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ArcTransitionToManagedTest() = default;
  ~ArcTransitionToManagedTest() override = default;
  ArcTransitionToManagedTest(const ArcTransitionToManagedTest&) = delete;
  ArcTransitionToManagedTest& operator=(const ArcTransitionToManagedTest&) =
      delete;

  void SetUp() override {
    ArcSessionManagerTest::SetUp();
    ArcSessionManager::SetUiEnabledForTesting(true);

    const std::string profile_name = profile()->GetProfileUserName();
    identity_test_environment_.MakeAccountAvailable(profile_name);
    signin::IdentityManager* identity_manager =
        identity_test_environment_.identity_manager();
    CoreAccountId account_id = identity_manager->PickAccountIdForAccount(
        signin::GetTestGaiaIdForEmail(profile_name), profile_name);

    // Inject a fake AndroidManagementClient to return MANAGED as the result.
    arc_session_manager()->SetAndroidManagementCheckerFactoryForTesting(
        base::BindLambdaForTesting([=](Profile* profile, bool retry_on_error) {
          auto fake_client =
              std::make_unique<policy::FakeAndroidManagementClient>();
          fake_client->SetResult(
              policy::AndroidManagementClient::Result::MANAGED);

          return std::make_unique<ArcAndroidManagementChecker>(
              profile, identity_manager, account_id, retry_on_error,
              std::move(fake_client));
        }));
  }

  bool transition_feature_enabled() const { return std::get<0>(GetParam()); }

  bool user_become_managed() const { return std::get<1>(GetParam()); }

  bool ShouldArcTransitionToManaged() const {
    return transition_feature_enabled() && user_become_managed();
  }

 protected:
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_P(ArcTransitionToManagedTest, TransitionFlow) {
  // Initialize feature state.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kEnableUnmanagedToManagedTransitionFeature,
                                    transition_feature_enabled());
  // Set up the situation that provisioning is successfully done in the
  // previous session.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

  // Initialize ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->AllowActivation();
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Emulate user management state change.
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      user_become_managed());
  base::RunLoop().RunUntilIdle();

  // Verify ARC state and ARC transition value.
  EXPECT_EQ(profile()->GetPrefs()->GetBoolean(prefs::kArcEnabled),
            ShouldArcTransitionToManaged());
  EXPECT_EQ(arc::GetManagementTransition(profile()),
            ShouldArcTransitionToManaged()
                ? arc::ArcManagementTransition::UNMANAGED_TO_MANAGED
                : arc::ArcManagementTransition::NO_TRANSITION);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ArcTransitionToManagedTest,
    testing::Combine(testing::Bool() /* transition_feature_enabled */,
                     testing::Bool() /* user_become_managed */));

}  // namespace
}  // namespace arc
