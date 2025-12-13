// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/session/logout_confirmation_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/uuid.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/constants/pref_names.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/fake_desk_sync_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash::floating_workspace {

namespace {

constexpr char kTestAccount[] = "usertest@gmail.com";
constexpr GaiaId::Literal kFakeGaia("fakegaia");
constexpr char kTestAccount2[] = "usertest2@gmail.com";
constexpr GaiaId::Literal kFakeGaia2("fakegaia2");

// Creates an app_restore::RestoreData object with `num_windows.size()` apps,
// where the ith app has `num_windows[i]` windows. The windows
// activation index is its creation order.
std::unique_ptr<app_restore::RestoreData> CreateRestoreData(
    std::vector<int> num_windows) {
  auto restore_data = std::make_unique<app_restore::RestoreData>();
  int32_t activation_index_counter = 0;
  for (size_t i = 0; i < num_windows.size(); ++i) {
    const std::string app_id = base::NumberToString(i);

    for (int32_t window_id = 0; window_id < num_windows[i]; ++window_id) {
      restore_data->AddAppLaunchInfo(
          std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id));

      app_restore::WindowInfo window_info;
      window_info.activation_index =
          std::make_optional<int32_t>(activation_index_counter++);

      restore_data->ModifyWindowInfo(app_id, window_id, window_info);
    }
  }
  return restore_data;
}

std::unique_ptr<ash::DeskTemplate> MakeTestFloatingWorkspaceDeskTemplate(
    std::string name,
    base::Time creation_time) {
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          base::Uuid::GenerateRandomV4(), ash::DeskTemplateSource::kUser, name,
          creation_time, DeskTemplateType::kFloatingWorkspace);
  std::unique_ptr<app_restore::RestoreData> restore_data =
      CreateRestoreData(std::vector<int>(10, 1));
  desk_template->set_desk_restore_data(std::move(restore_data));
  return desk_template;
}

class MockDesksClient : public DesksClient {
 public:
  MockDesksClient() = default;
  MOCK_METHOD((base::expected<std::vector<const ash::Desk*>, DeskActionError>),
              GetAllDesks,
              (),
              (override));
  MOCK_METHOD((std::optional<DesksClient::DeskActionError>),
              RemoveDesk,
              (const base::Uuid& desk_uuid, ash::DeskCloseType close_type),
              (override));
  MOCK_METHOD((base::Uuid), GetActiveDesk, (), (override));

  void CaptureActiveDesk(CaptureActiveDeskAndSaveTemplateCallback callback,
                         ash::DeskTemplateType template_type) override {
    std::move(callback).Run(std::nullopt, captured_desk_template_ != nullptr
                                              ? captured_desk_template_->Clone()
                                              : nullptr);
  }

  void LaunchAppsFromTemplate(
      std::unique_ptr<ash::DeskTemplate> desk_template) override {
    restored_template_uuid_ = desk_template->uuid();
    restored_desk_template_ = std::move(desk_template);
  }

  void LaunchDeskTemplate(
      const base::Uuid& template_uuid,
      LaunchDeskCallback callback,
      const std::u16string& customized_desk_name = std::u16string()) override {
    restored_template_uuid_ = template_uuid;
    std::move(callback).Run(std::nullopt, base::Uuid::GenerateRandomV4());
  }

  DeskTemplate* restored_desk_template() {
    return restored_desk_template_.get();
  }

  base::Uuid& restored_template_uuid() { return restored_template_uuid_; }

  void SetCapturedDeskTemplate(
      std::unique_ptr<const DeskTemplate> captured_template) {
    captured_desk_template_ = std::move(captured_template);
  }

 private:
  std::unique_ptr<const DeskTemplate> captured_desk_template_;
  std::unique_ptr<DeskTemplate> restored_desk_template_;
  base::Uuid restored_template_uuid_;
};

}  // namespace

class FloatingWorkspaceServiceTest : public testing::Test {
 public:
  FloatingWorkspaceServiceTest() = default;

  ~FloatingWorkspaceServiceTest() override = default;

  TestingProfile* profile() const { return profile_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  desks_storage::FakeDeskSyncService* fake_desk_sync_service() {
    return fake_desk_sync_service_.get();
  }

  syncer::TestSyncService* test_sync_service() { return &test_sync_service_; }

  ui::UserActivityDetector* user_activity_detector() {
    return ui::UserActivityDetector::Get();
  }

  user_manager::FakeUserManager* fake_user_manager() const {
    return fake_user_manager_.Get();
  }

  MockDesksClient* mock_desks_client() { return mock_desks_client_.get(); }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  NetworkHandlerTestHelper* network_handler_test_helper() {
    return network_handler_test_helper_.get();
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  void AddTestNetworkDevice() {
    network_handler_test_helper_->AddDefaultProfiles();
  }

  void CleanUpTestNetworkDevices() {
    network_handler_test_helper_->ClearDevices();
    network_handler_test_helper_->ClearServices();
    network_handler_test_helper_->ClearProfiles();
  }

  apps::AppRegistryCache* cache() { return cache_.get(); }

  AccountId& account_id() { return account_id_; }

  AshTestHelper* ash_test_helper() { return &ash_test_helper_; }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client(
        base::PassKey<floating_workspace::FloatingWorkspaceServiceTest>());
  }

  // We want to hold off on populating the apps cache before each test is run
  // because the list of initialization types do not get reset. To test that the
  // service is actually waiting for the app types to initialize, we need to
  // keep it empty before then. For all other tests, this needs to be called
  // before we get the `kUpToDate` from the sync service.
  void PopulateAppsCache() {
    desks_storage::desk_test_util::PopulateFloatingWorkspaceAppRegistryCache(
        account_id_, cache_.get());
    task_environment_.RunUntilIdle();
  }

  // TODO(crbug.com/400730387): add proper variations to all tests in this file
  // to account for differences between first and consequent sync scenarios. On
  // the first sync `FloatingWorkspaceService` can open the desk once we get
  // Sync data via `MergeFullSyncData` method of the bridge. On consequent syncs
  // we are waiting for `UpToDate` signal from the sync server instead. Tests in
  // this file were written when we could only rely on `UpToDate` signal. In
  // these tests we don't mock the `MergeFullSyncData` method and by default our
  // fake desk sync service executes the launch callback as soon as it is set
  // from `FloatingWorkspaceService`. `SkipOnFirstSyncCallback` is a temporary
  // workaround which allows to skip the execution of this callback in selected
  // tests. It is mostly needed for tests which imitate different delay
  // scenarios.
  void SkipOnFirstSyncCallback() {
    fake_desk_sync_service()->skip_on_first_sync_callback_ = true;
  }

  syncer::UploadState GetSyncUploadState(syncer::DataType data_type) {
    return syncer::GetUploadToGoogleState(test_sync_service(), data_type);
  }

  void CreateFloatingWorkspaceServiceForTesting(TestingProfile* profile) {
    FloatingWorkspaceServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile,
        base::BindLambdaForTesting([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<FloatingWorkspaceService>(
              Profile::FromBrowserContext(context));
        }));
    task_environment_.RunUntilIdle();
  }

  FloatingWorkspaceService* InitFloatingWorkspaceServiceAndStartSession() {
    auto* floating_workspace_service =
        FloatingWorkspaceServiceFactory::GetForProfile(profile());
    floating_workspace_service->Init(test_sync_service(),
                                     fake_desk_sync_service());
    EXPECT_TRUE(floating_workspace_service->IsObservingForTesting());
    // TODO(crbug.com/419250389): we should properly mimic entering user session
    // instead of just calling these methods manually.
    session_manager::SessionManager::Get()
        ->HandleUserSessionStartUpTaskCompleted();
    floating_workspace_service->OnFirstSessionReady();
    return floating_workspace_service;
  }

  FloatingWorkspaceService* InitAndPrepareTemplateForCapture(
      const std::string& template_name,
      base::Time creation_time) {
    PopulateAppsCache();
    CreateFloatingWorkspaceServiceForTesting(profile());
    auto* floating_workspace_service =
        FloatingWorkspaceServiceFactory::GetForProfile(profile());
    floating_workspace_service->Init(test_sync_service(),
                                     fake_desk_sync_service());
    std::unique_ptr<DeskTemplate> floating_workspace_template =
        MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
    test_sync_service()->SetDownloadStatusFor(
        {syncer::DataType::WORKSPACE_DESK},
        syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
    test_sync_service()->FireStateChanged();
    mock_desks_client()->SetCapturedDeskTemplate(
        std::move(floating_workspace_template));
    return floating_workspace_service;
  }

  bool WaitForStartupDialogToClose() {
    return base::test::RunUntil(
        []() { return !FloatingWorkspaceDialog::IsShown(); });
  }

  bool WaitForNetworkScreenToAppear() {
    task_environment().FastForwardBy(ash::kFwsNetworkScreenDelay);
    return base::test::RunUntil([]() {
      return FloatingWorkspaceDialog::IsShown() ==
             FloatingWorkspaceDialog::State::kNetwork;
    });
  }

  void CloseStartupDialogIfNeeded() {
    if (!FloatingWorkspaceDialog::IsShown()) {
      return;
    }
    FloatingWorkspaceDialog::Close();
    EXPECT_TRUE(WaitForStartupDialogToClose());
  }

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    cros_settings_test_helper_ =
        std::make_unique<ScopedCrosSettingsTestHelper>();
    ash::AshTestHelper::InitParams params;
    ash_test_helper_.SetUp(std::move(params));
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>(
        TestingBrowserProcess::GetGlobal()->local_state()));
    account_id_ = AccountId::FromUserEmailGaiaId(kTestAccount, kFakeGaia);
    fake_user_manager()->AddGaiaUser(account_id_,
                                     user_manager::UserType::kRegular);
    fake_user_manager()->UserLoggedIn(
        account_id_,
        user_manager::TestHelper::GetFakeUsernameHash(account_id_));
    CoreAccountInfo account_info;
    account_info.email = kTestAccount;
    account_info.gaia = GaiaId("gaia");
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);
    // By default, TestSyncService sets the status `kUpToDate` for all types.
    // Make sure that we start from `kWaitingForUpdates` instead so that each
    // test can then control precisely when Sync data becomes up to date.
    test_sync_service_.SetDownloadStatusFor(
        {syncer::DataType::WORKSPACE_DESK},
        syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
    test_sync_service_.SetDownloadStatusFor(
        {syncer::DataType::COOKIES},
        syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    auto* prefs_ptr = prefs.get();
    profile_ = profile_manager_->CreateTestingProfile(
        kTestAccount, std::move(prefs), std::u16string(), /*avatar_id=*/0,
        TestingProfile::TestingFactories());
    prefs_ptr->SetManagedPref(chromeos::prefs::kFloatingWorkspaceV2Enabled,
                              base::Value(true));
    fake_user_manager()->OnUserProfileCreated(account_id_, prefs_ptr);
    fake_desk_sync_service_ =
        std::make_unique<desks_storage::FakeDeskSyncService>(
            /*skip_engine_connection=*/true);
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    AddTestNetworkDevice();
    test_sync_service()->SetDownloadStatusFor(
        {syncer::DataType::DEVICE_INFO},
        syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
    user_activity_detector()->set_last_activity_time_for_test(
        base::TimeTicks::Now());
    cache_ = std::make_unique<apps::AppRegistryCache>();
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             cache_.get());
    mock_desks_client_ = std::make_unique<MockDesksClient>();

    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(
        web_contents_factory_->CreateWebContents(profile_));
    auto ui = std::make_unique<FloatingWorkspaceUI>(test_web_ui_.get());
    test_web_ui_->SetController(std::move(ui));
  }

  void TearDown() override {
    CloseStartupDialogIfNeeded();
    test_web_ui_.reset();
    web_contents_factory_.reset();
    auto* floating_workspace_service =
        FloatingWorkspaceServiceFactory::GetForProfile(profile());
    if (floating_workspace_service) {
      floating_workspace_service->ShutDownServicesAndObservers();
    }
    fake_user_manager()->OnUserProfileWillBeDestroyed(account_id_);
    profile_ = nullptr;
    profile_manager_ = nullptr;
    mock_desks_client_ = nullptr;
    fake_user_manager_.Reset();
    ash_test_helper_.TearDown();
    cros_settings_test_helper_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
  std::unique_ptr<ScopedCrosSettingsTestHelper> cros_settings_test_helper_;
  AshTestHelper ash_test_helper_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<MockDesksClient> mock_desks_client_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  ash::SessionTerminationManager session_termination_manager_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;

  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(FloatingWorkspaceServiceTest, RestoreFloatingWorkspaceTemplate) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceTest,
       RestoreWhenInitializedAfterRelevantSyncStateChanges) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  // Get all the data from Sync before the service is initialized.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  CreateFloatingWorkspaceServiceForTesting(profile());
  // Initialize the service and verify that the desk is restored without waiting
  // for any additional events from Sync.
  InitFloatingWorkspaceServiceAndStartSession();

  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceTest, NoNetworkOnFloatingWorkspaceInit) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* service = InitFloatingWorkspaceServiceAndStartSession();
  // We always show the default UI first and then show the network screen (if
  // still needed) after a short delay, to account for possible race condition
  // between initializing FloatingWorkspaceService and connecting to network
  // when entering the session.
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
  WaitForNetworkScreenToAppear();

  // Checking that after any network update that doesn't make us online the
  // flow doesn't start from the beginning, i.e. we stay on the network screen.
  service->NetworkConnectionStateChanged(nullptr);
  EXPECT_EQ(FloatingWorkspaceDialog::State::kNetwork,
            FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest, NetworkConnectingShortlyAfterFwsInit) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  task_environment().RunUntilIdle();
  task_environment().FastForwardBy(ash::kFwsNetworkScreenDelay);
  // We went online in the short delay before showing the network screen -
  // verify that we are still showing the default UI as a result.
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest, NetworkConnectedButOffline) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  // Connect to wifi, but set it to the ready state instead of online.
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  std::string path = network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  task_environment().RunUntilIdle();
  ASSERT_TRUE(NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  WaitForNetworkScreenToAppear();

  // Switch wifi to online and check that Floating Workspace service
  // detects it and switches the startup UI back to default.
  network_handler_test_helper()->SetServiceProperty(
      path, shill::kStateProperty, base::Value(shill::kStateOnline));
  task_environment().RunUntilIdle();
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest,
       NoNetworkForFloatingWorkspaceTemplateAfterLongDelay) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  task_environment().RunUntilIdle();

  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());

  task_environment().FastForwardBy(base::Seconds(30));
  CleanUpTestNetworkDevices();
  task_environment().RunUntilIdle();
  WaitForNetworkScreenToAppear();
}

TEST_F(FloatingWorkspaceServiceTest, ConnectAfterNotHavingNetworkInitially) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();
  WaitForNetworkScreenToAppear();

  // While offline, Sync might report that download status is up to date, while
  // upload state indicates we are not active yet. Check that we are not
  // restoring anything in that case.
  test_sync_service()->SetEmptyLastCycleSnapshot();
  ASSERT_NE(syncer::UploadState::ACTIVE,
            GetSyncUploadState(syncer::DataType::WORKSPACE_DESK));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());

  // Go online.
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  task_environment().RunUntilIdle();
  floating_workspace_service->DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());

  // Just going online is not enough - wait for a sync cycle to complete.
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());

  test_sync_service()->SetNonEmptyLastCycleSnapshot();
  ASSERT_EQ(syncer::UploadState::ACTIVE,
            GetSyncUploadState(syncer::DataType::WORKSPACE_DESK));
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceTest,
       PreventNetworkIssueNotifFromFiringAfterRestoreAttemptOrRestoreHappened) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Disconnect from internet. Make sure no UI is shown since restore
  // happened already.
  CleanUpTestNetworkDevices();
  task_environment().RunUntilIdle();
  EXPECT_FALSE(FloatingWorkspaceDialog::IsShown());
  // Add network back and make sure there is still no UI.
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  floating_workspace_service->DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  EXPECT_FALSE(FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest,
       NoNetworkUiLogicWhenSyncIsInactiveAndOnceSyncIsActiveAgain) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  ASSERT_FALSE(test_sync_service()->IsSyncFeatureEnabled());
  InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(FloatingWorkspaceDialog::State::kError,
            FloatingWorkspaceDialog::IsShown());
  test_sync_service()->SetAllowedByEnterprisePolicy(true);
  ASSERT_TRUE(test_sync_service()->IsSyncFeatureEnabled());
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateLoadMetric) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  base::HistogramTester histogram_tester;
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateLoadTime,
      1u);
}

TEST_F(FloatingWorkspaceServiceTest, CaptureFloatingWorkspaceTemplate) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));

  // Check that we don't upload a desk until restore happens.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // No upload from waiting.
  ASSERT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  ash::Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  // No upload from clicking on the tray.
  ASSERT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());

  // Once we get the signal which triggers restore, capture and upload will
  // start happening.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceTest, CaptureSameFloatingWorkspaceTemplate) {
  // Upload should be skipped if two captured templates are the same.

  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(first_captured_floating_workspace_template));
  // Trigger the first capture task.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  const base::Time second_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> second_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, second_captured_template_creation_time);

  // Set the 2nd template to be captured.
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(second_captured_floating_workspace_template));
  // Fast forward by capture interval capture a second time.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));

  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  // Second captured template is the same as first, template should not be
  // updated, creation time is first template's creation time.
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            first_captured_template_creation_time);
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureDifferentFloatingWorkspaceTemplate) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(first_captured_floating_workspace_template));
  // Trigger the first capture task.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  const base::Time second_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> second_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, second_captured_template_creation_time);

  // Create new restore data different than 1st captured one.
  std::unique_ptr<app_restore::RestoreData> restore_data =
      CreateRestoreData(std::vector<int>(11, 1));
  second_captured_floating_workspace_template->set_desk_restore_data(
      std::move(restore_data));
  // Set the 2nd template to be captured.
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(second_captured_floating_workspace_template));
  // Fast forward by capture interval capture a second time.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));

  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  // Second captured template has different restore data than first, template
  // should be updated, replacing the first one.
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            second_captured_template_creation_time);
}

TEST_F(FloatingWorkspaceServiceTest, PopulateFloatingWorkspaceTemplate) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      floating_workspace_service->GetFloatingWorkspaceTemplateEntries().size(),
      1u);
}

TEST_F(FloatingWorkspaceServiceTest,
       PopulateFloatingWorkspaceTemplateWithUpdates) {
  PopulateAppsCache();
  std::unique_ptr<ash::DeskTemplate> template_1 =
      MakeTestFloatingWorkspaceDeskTemplate("Template 1", base::Time::Now());
  base::Uuid template_1_uuid = template_1->uuid();
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      std::move(template_1),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      floating_workspace_service->GetFloatingWorkspaceTemplateEntries().size(),
      1u);

  std::unique_ptr<ash::DeskTemplate> template_2 =
      MakeTestFloatingWorkspaceDeskTemplate("Template 2", base::Time::Now());
  base::Uuid template_2_uuid = template_2->uuid();
  base::RunLoop loop2;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      std::move(template_2),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop2.Quit();
          }));
  loop2.Run();
  base::RunLoop loop3;
  fake_desk_sync_service()->GetDeskModel()->DeleteEntry(
      template_1_uuid,
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::DeleteEntryStatus status) {
            EXPECT_EQ(desks_storage::DeskModel::DeleteEntryStatus::kOk, status);
            loop3.Quit();
          }));
  loop3.Run();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      floating_workspace_service->GetFloatingWorkspaceTemplateEntries().size(),
      1u);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(floating_workspace_service->GetFloatingWorkspaceTemplateEntries()[0]
                ->uuid(),
            template_2_uuid);
}

TEST_F(FloatingWorkspaceServiceTest,
       DoNotPerformGarbageCollectionOnSingleEntryBeyondThreshold) {
  PopulateAppsCache();
  const std::string fws_name = "Template 1";
  std::unique_ptr<ash::DeskTemplate> fws_template =
      MakeTestFloatingWorkspaceDeskTemplate(fws_name, base::Time::Now());
  fws_template->set_client_cache_guid("cache_guid_1");
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      std::move(fws_template),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  task_environment().AdvanceClock(base::Days(31));
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(fws_name));

  EXPECT_EQ(
      1ul, fake_desk_sync_service()->GetDeskModel()->GetAllEntryUuids().size());
}

TEST_F(FloatingWorkspaceServiceTest, PerformGarbageCollectionOnStaleEntries) {
  PopulateAppsCache();
  const std::string fws_one_name = "Template 1";
  const std::string fws_two_name = "Template 2";
  std::unique_ptr<ash::DeskTemplate> fws_one =
      MakeTestFloatingWorkspaceDeskTemplate(fws_one_name, base::Time::Now());
  fws_one->set_client_cache_guid("cache_guid_1");
  std::unique_ptr<ash::DeskTemplate> fws_two =
      MakeTestFloatingWorkspaceDeskTemplate(fws_two_name,
                                            base::Time::Now() + base::Days(2));
  fws_two->set_client_cache_guid("cache_guid_2");
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      std::move(fws_one),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  base::RunLoop loop2;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      std::move(fws_two),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop2.Quit();
          }));
  loop2.Run();
  task_environment().AdvanceClock(base::Days(31));
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();

  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(fws_two_name));

  EXPECT_EQ(
      1ul, fake_desk_sync_service()->GetDeskModel()->GetAllEntryUuids().size());
}

TEST_F(FloatingWorkspaceServiceTest, FloatingWorkspaceShowsStartupUi) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());

  // Wait for download to complete and check that the UI is gone.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(WaitForStartupDialogToClose());
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateUiSwitchOnSyncError) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_EQ(FloatingWorkspaceDialog::State::kDefault,
            FloatingWorkspaceDialog::IsShown());
  // Send sync error to service.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kError);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(FloatingWorkspaceDialog::State::kError,
            FloatingWorkspaceDialog::IsShown());
}

TEST_F(FloatingWorkspaceServiceTest,
       RestoreWhenNoFloatingWorkspaceTemplateIsAvailable) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
}

TEST_F(FloatingWorkspaceServiceTest, NoRestoreIfTabSyncIsDisabled) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();

  // Disable tab sync before initializing FloatingWorkspaceService.
  syncer::UserSelectableTypeSet types_to_enable =
      test_sync_service()->GetUserSettings()->GetSelectedTypes();
  ASSERT_TRUE(types_to_enable.Has(syncer::UserSelectableType::kTabs));
  types_to_enable.Remove(syncer::UserSelectableType::kTabs);
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, types_to_enable);

  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  // No restore is expected when tab sync is disabled.
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
}

TEST_F(FloatingWorkspaceServiceTest, CaptureBasedOnTabSyncSetting) {
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  auto* floating_workspace_service =
      InitAndPrepareTemplateForCapture(template_name, creation_time);

  // Disable tab sync.
  syncer::UserSelectableTypeSet types_to_enable =
      test_sync_service()->GetUserSettings()->GetSelectedTypes();
  ASSERT_TRUE(types_to_enable.Has(syncer::UserSelectableType::kTabs));
  types_to_enable.Remove(syncer::UserSelectableType::kTabs);
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, types_to_enable);
  test_sync_service()->FireStateChanged();

  // Wait until the time when the template capture should have been triggered,
  // and check that it didn't happen.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  EXPECT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());

  // Typically we also trigger a capture on system tray bubble being show. Check
  // that this code path also respects the tab sync setting.
  ash::Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  EXPECT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());

  // Enable tab sync and verify that we start capturing again.
  test_sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kTabs});
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateNotFoundMetric) {
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateNotFound,
      1u);
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordFloatingWorkspaceV2InitMetric) {
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2Initialized, 1u);
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureButDontUploadIfNoUserActionAfterkUpToDate) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Idle for a while.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(first_captured_floating_workspace_template));
  // Trigger the first capture task.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));

  EXPECT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
}

TEST_F(FloatingWorkspaceServiceTest,
       WaitForAppCacheBeforeRestoringFloatingWorkspaceTemplate) {
  apps::AppRegistryCacheWrapper& wrapper = apps::AppRegistryCacheWrapper::Get();
  wrapper.RemoveAppRegistryCache(cache());
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();

  wrapper.AddAppRegistryCache(account_id(), cache());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  PopulateAppsCache();
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureButDontUploadIfNoUserActionAfterLastUpload) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  // Idle for a while.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(first_captured_floating_workspace_template));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Trigger the first capture task.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));

  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());

  const std::string template_name2 = "floating_workspace_captured_template_2";
  const base::Time second_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> second_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name2, second_captured_template_creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(second_captured_floating_workspace_template));
  // Trigger the second capture task.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                  ->template_name() != base::UTF8ToUTF16(template_name2));
}

TEST_F(FloatingWorkspaceServiceTest, CaptureImmediatelyAfterRestore) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();

  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now() + base::Milliseconds(1));
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureFloatingWorkspaceTemplateOnSystemTrayVisible) {
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  auto* floating_workspace_service =
      InitAndPrepareTemplateForCapture(template_name, creation_time);
  ash::Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureFloatingWorkspaceTemplateOnSignOutConfirmation) {
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  auto* floating_workspace_service =
      InitAndPrepareTemplateForCapture(template_name, creation_time);
  // Confirmation is only required when we set a non-zero `logout_time` to
  // `LogoutConfirmationController::ConfirmLogout`.
  base::TimeDelta non_zero_logout_confirmation_duration = base::Seconds(20);
  ash::Shell::Get()->logout_confirmation_controller()->ConfirmLogout(
      base::TimeTicks::Now() + non_zero_logout_confirmation_duration,
      ash::LogoutConfirmationController::Source::kShelfExitButton);
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureFloatingWorkspaceTemplateWhenLockingTheScreen) {
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  client.Init();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Set the captured desk template after the sync service has fire the
  // `kUpToDate` signal. This is because a capture and upload happens after the
  // fire event. We want to instead set the captured template after this so we
  // can test that a new template was captured and uploaded.
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  base::RunLoop run_loop;
  client.PrepareForLock(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceTest, DontUploadEmptyDeskFromLockScreen) {
  SkipOnFirstSyncCallback();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  // Create and capture a template
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);

  // Simulate screen lock and verify that the session state is reported as
  // locked.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  GetSessionControllerClient()->FlushForTest();
  ASSERT_EQ(session_manager::SessionState::LOCKED,
            ash::Shell::Get()->session_controller()->GetSessionState());

  // Wait for the next capture and verify that we didn't capture an empty desk.
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  const DeskTemplate* current_template =
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate();
  const app_restore::RestoreData* restore_data =
      current_template->desk_restore_data();
  EXPECT_FALSE(restore_data->app_id_to_launch_list().empty());
}

TEST_F(FloatingWorkspaceServiceTest,
       RestoreAfterWakingUpFromSleepWithSyncUpdatesAfterUnlock) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));

  // Send device to sleep. Add a newly captured floating workspace template.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  const std::string new_template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now() + base::Seconds(1);
  std::unique_ptr<DeskTemplate> new_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(new_template_name, creation_time);
  base::RunLoop loop2;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      new_floating_workspace_template->Clone(),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop2.Quit();
          }));
  loop2.Run();
  // Wake device up and unlock it.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
  // Receive Sync updates and verify that they lead to restoration of the new
  // template.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_template_uuid().is_valid());
  EXPECT_EQ(mock_desks_client()->restored_template_uuid(),
            new_floating_workspace_template->uuid());
}

TEST_F(FloatingWorkspaceServiceTest,
       RestoreAfterWakingUpFromSleepWithSyncUpdatesBeforeUnlock) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));

  // Send device to sleep. Add a newly captured floating workspace template.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  const std::string new_template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now() + base::Seconds(1);
  std::unique_ptr<DeskTemplate> new_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(new_template_name, creation_time);
  base::RunLoop loop2;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      new_floating_workspace_template->Clone(),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop2.Quit();
          }));
  loop2.Run();
  // Wake device up.
  power_manager_client()->SendSuspendDone();
  // Send Sync updates while we are on the lock screen.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  // Check that behind the lock screen we still have the old desk open.
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Unlock the screen.
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
  // Check that the new desk gets opened without additional notifications from
  // Sync.
  ASSERT_TRUE(mock_desks_client()->restored_template_uuid().is_valid());
  EXPECT_EQ(mock_desks_client()->restored_template_uuid(),
            new_floating_workspace_template->uuid());
}

class FloatingWorkspaceServiceMultiUserTest
    : public FloatingWorkspaceServiceTest {
 protected:
  FloatingWorkspaceServiceMultiUserTest() = default;
  ~FloatingWorkspaceServiceMultiUserTest() override { profile2_ = nullptr; }

  TestingProfile* profile2() { return profile2_.get(); }

  AccountId account_id2() { return account_id2_; }

  apps::AppRegistryCache* cache2() { return cache2_.get(); }

  desks_storage::FakeDeskSyncService* fake_desk_sync_service2() {
    return fake_desk_sync_service2_.get();
  }

  syncer::TestSyncService* test_sync_service2() {
    return test_sync_service2_.get();
  }

  void PopulateAppsCache2() {
    desks_storage::desk_test_util::PopulateFloatingWorkspaceAppRegistryCache(
        account_id2_, cache2_.get());
    task_environment().RunUntilIdle();
  }

  void SetUp() override {
    FloatingWorkspaceServiceTest::SetUp();
    EXPECT_TRUE(temp_dir2_.CreateUniqueTempDir());
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    profile2_ = profile_manager()->CreateTestingProfile(
        kTestAccount2, std::move(prefs), std::u16string(),
        /*avatar_id=*/0, TestingProfile::TestingFactories());

    account_id2_ = AccountId::FromUserEmailGaiaId(kTestAccount2, kFakeGaia2);
    fake_user_manager()->AddGaiaUser(account_id2_,
                                     user_manager::UserType::kRegular);
    fake_user_manager()->UserLoggedIn(
        account_id2_,
        user_manager::TestHelper::GetFakeUsernameHash(account_id2_));
    CoreAccountInfo account_info;
    account_info.email = kTestAccount2;
    account_info.gaia = GaiaId("gaia2");
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync, account_info);
    fake_desk_sync_service2_ =
        std::make_unique<desks_storage::FakeDeskSyncService>(
            /*skip_engine_connection=*/true);
    test_sync_service2_ = std::make_unique<syncer::TestSyncService>();

    cache2_ = std::make_unique<apps::AppRegistryCache>();
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id2_,
                                                             cache2_.get());
  }

  void TearDown() override {
    auto* floating_workspace_service2 =
        FloatingWorkspaceServiceFactory::GetForProfile(profile2());
    if (floating_workspace_service2) {
      floating_workspace_service2->ShutDownServicesAndObservers();
    }
    profile2_ = nullptr;
    FloatingWorkspaceServiceTest::TearDown();
  }

 private:
  std::unique_ptr<syncer::TestSyncService> test_sync_service2_;
  std::unique_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service2_;
  base::ScopedTempDir temp_dir2_;
  AccountId account_id2_;
  std::unique_ptr<apps::AppRegistryCache> cache2_;
  raw_ptr<TestingProfile> profile2_ = nullptr;
};

TEST_F(FloatingWorkspaceServiceMultiUserTest, TwoUserLoggedInAndCaptureStops) {
  PopulateAppsCache();
  PopulateAppsCache2();
  CreateFloatingWorkspaceServiceForTesting(profile());
  CreateFloatingWorkspaceServiceForTesting(profile2());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();
  auto* floating_workspace_service2 =
      FloatingWorkspaceServiceFactory::GetForProfile(profile2());
  floating_workspace_service2->Init(test_sync_service2(),
                                    fake_desk_sync_service2());
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  test_sync_service2()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service2()->FireStateChanged();

  fake_user_manager()->SwitchActiveUser(account_id());
  // Capture a desk template and upload to current account.
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  // Verify that it has been uploaded.
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->template_name(),
            base::UTF8ToUTF16(template_name));

  // Switch accounts and capture a desk template.
  const std::string template_name2 = "floating_workspace_captured_template";
  const base::Time creation_time2 = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template2 =
      MakeTestFloatingWorkspaceDeskTemplate(template_name2, creation_time2);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template2));
  fake_user_manager()->SwitchActiveUser(account_id2());
  floating_workspace_service->OnActiveUserSessionChanged(account_id2());
  task_environment().RunUntilIdle();
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Verify that the latest captured template was before the switch.
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceMultiUserTest,
       TwoUserLoggedInAndUploadsToCorrectAccount) {
  PopulateAppsCache();
  PopulateAppsCache2();
  fake_user_manager()->SwitchActiveUser(account_id());
  CreateFloatingWorkspaceServiceForTesting(profile());
  CreateFloatingWorkspaceServiceForTesting(profile2());
  FloatingWorkspaceService* floating_workspace_service =
      InitFloatingWorkspaceServiceAndStartSession();
  auto* floating_workspace_service2 =
      FloatingWorkspaceServiceFactory::GetForProfile(profile2());
  floating_workspace_service2->Init(test_sync_service2(),
                                    fake_desk_sync_service2());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  test_sync_service2()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service2()->FireStateChanged();

  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  task_environment().FastForwardBy(ash::kFwsPeriodicJobInterval +
                                   base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_FALSE(
      floating_workspace_service2->GetLatestFloatingWorkspaceTemplate());
}

class FloatingWorkspaceServiceV2WithCookiesTest
    : public FloatingWorkspaceServiceTest {
 protected:
  FloatingWorkspaceServiceV2WithCookiesTest() = default;
  ~FloatingWorkspaceServiceV2WithCookiesTest() override = default;

  void SetUp() override {
    FloatingWorkspaceServiceTest::SetUp();
    // Set prefs needed for Floating SSO feature (which syncs cookies).
    profile()->GetPrefs()->SetBoolean(chromeos::prefs::kFloatingSsoEnabled,
                                      true);
    profile()->GetPrefs()->SetBoolean(syncer::prefs::internal::kSyncManaged,
                                      false);
    profile()->GetPrefs()->SetBoolean(
        syncer::prefs::internal::kSyncKeepEverythingSynced, true);
  }
};

TEST_F(FloatingWorkspaceServiceV2WithCookiesTest,
       RestoreTemplateAfterWaitingForCookies) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::COOKIES},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  // Verify that there is no restored desk template yet: when Floating SSO is
  // enabled, we also wait for cookies to be up to date.
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::COOKIES},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  // Desk template is restored once cookies are up to date.
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceV2WithCookiesTest,
       RestoreTemplateWhenCookiesHaveSyncError) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(template_name, base::Time::Now()),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  CreateFloatingWorkspaceServiceForTesting(profile());
  InitFloatingWorkspaceServiceAndStartSession();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::COOKIES},
      syncer::SyncService::DataTypeDownloadStatus::kError);
  test_sync_service()->FireStateChanged();
  // Desk template is restored without waiting for Floating SSO if Sync reports
  // an error for cookies.
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

class FloatingWorkspaceServiceSafeModeTest
    : public FloatingWorkspaceServiceTest {
 protected:
  FloatingWorkspaceServiceSafeModeTest() = default;
  ~FloatingWorkspaceServiceSafeModeTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kSafeMode);
    FloatingWorkspaceServiceTest::SetUp();
  }
};

TEST_F(FloatingWorkspaceServiceSafeModeTest, NoFwsInSafeMode) {
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service());
  EXPECT_FALSE(floating_workspace_service->IsObservingForTesting());
}

}  // namespace ash::floating_workspace
