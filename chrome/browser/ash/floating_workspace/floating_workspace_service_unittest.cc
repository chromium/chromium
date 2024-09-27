// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/session/test_session_controller.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/fake_desk_sync_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::floating_workspace {

namespace {

constexpr char kLocalSessionName[] = "local_session";
constexpr char kRemoteSessionOneName[] = "remote_session_1";
constexpr char kRemoteSession2Name[] = "remote_session_2";
constexpr char kTestAccount[] = "usertest@gmail.com";
constexpr char kTestAccount2[] = "usertest2@gmail.com";
const base::Time most_recent_time = base::Time::FromSecondsSinceUnixEpoch(15);
const base::Time more_recent_time = base::Time::FromSecondsSinceUnixEpoch(10);
const base::Time least_recent_time = base::Time::FromSecondsSinceUnixEpoch(5);
std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const base::Time& session_time) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  session->SetSessionName(session_name);
  session->SetModifiedTime(session_time);
  return session;
}

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

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, name, "chrome_version", "user_agent",
      sync_pb::SyncEnums::TYPE_UNSET, syncer::DeviceInfo::OsType::kUnknown,
      syncer::DeviceInfo::FormFactor::kUnknown, "device_id",
      "manufacturer_name", "model_name", "full_hardware_class",
      last_updated_timestamp, base::Minutes(60),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt, /*paask_info=*/std::nullopt, "token",
      syncer::DataTypeSet(),
      /*floating_workspace_last_signin_timestamp=*/last_updated_timestamp);
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

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() = default;

  bool GetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) override {
    *sessions = foreign_sessions_;
    base::ranges::sort(*sessions, std::greater(),
                       [](const sync_sessions::SyncedSession* session) {
                         return session->GetModifiedTime();
                       });

    return !sessions->empty();
  }

  bool GetLocalSession(
      const sync_sessions::SyncedSession** local_session) override {
    *local_session = local_session_;
    return *local_session != nullptr;
  }

  void SetForeignSessionsForTesting(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>> foreign_sessions) {
    foreign_sessions_ = foreign_sessions;
  }

  void SetLocalSessionForTesting(sync_sessions::SyncedSession* local_session) {
    local_session_ = local_session;
  }

  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));

  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));

  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));

  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));

 private:
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions_;
  raw_ptr<sync_sessions::SyncedSession, DanglingUntriaged> local_session_ =
      nullptr;
};

}  // namespace

class TestFloatingWorkSpaceService : public FloatingWorkspaceService {
 public:
  explicit TestFloatingWorkSpaceService(
      TestingProfile* profile,
      raw_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service,
      raw_ptr<syncer::TestSyncService> mock_sync_service,
      raw_ptr<syncer::FakeDeviceInfoSyncService> fake_device_info_sync_service,
      floating_workspace_util::FloatingWorkspaceVersion version)
      : FloatingWorkspaceService(profile, version) {
    Init(mock_sync_service, fake_desk_sync_service,
         fake_device_info_sync_service);
    mock_open_tabs_ = std::make_unique<MockOpenTabsUIDelegate>();
  }

  void RestoreLocalSessionWindows() override {
    mock_open_tabs_->GetLocalSession(&restored_session_.AsEphemeralRawAddr());
  }

  void RestoreForeignSessionWindows(
      const sync_sessions::SyncedSession* session) override {
    restored_session_ = session;
  }

  const sync_sessions::SyncedSession* GetRestoredSession() {
    return restored_session_.get();
  }

  void SetLocalSessionForTesting(sync_sessions::SyncedSession* session) {
    mock_open_tabs_->SetLocalSessionForTesting(session);
  }

  void SetForeignSessionForTesting(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>> foreign_sessions) {
    mock_open_tabs_->SetForeignSessionsForTesting(foreign_sessions);
  }

 private:
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return mock_open_tabs_.get();
  }

  void LaunchFloatingWorkspaceTemplate(
      const DeskTemplate* desk_template) override {
    restored_floating_workspace_template_ = desk_template;
  }

  raw_ptr<const sync_sessions::SyncedSession> restored_session_ = nullptr;
  raw_ptr<const DeskTemplate, DanglingUntriaged>
      restored_floating_workspace_template_ = nullptr;
  raw_ptr<DeskTemplate> uploaded_desk_template_ = nullptr;
  std::unique_ptr<MockOpenTabsUIDelegate> mock_open_tabs_;
};

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

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  NotificationDisplayServiceTester* display_service() {
    return display_service_.get();
  }

  syncer::TestSyncService* test_sync_service() { return &test_sync_service_; }

  ui::UserActivityDetector* user_activity_detector() {
    return ui::UserActivityDetector::Get();
  }

  syncer::FakeDeviceInfoSyncService* fake_device_info_sync_service() {
    return fake_device_info_sync_service_.get();
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

  bool HasNotificationFor(const std::string& id) {
    std::optional<message_center::Notification> notification =
        display_service()->GetNotification(id);
    return notification.has_value();
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

  void CreateFloatingWorkspaceServiceForTesting(TestingProfile* profile) {
    FloatingWorkspaceServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<FloatingWorkspaceService>(
              Profile::FromBrowserContext(context),
              floating_workspace_util::FloatingWorkspaceVersion::
                  kFloatingWorkspaceV2Enabled);
        }));
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::AshTestHelper::InitParams params;
    ash_test_helper_.SetUp(std::move(params));
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>(
        TestingBrowserProcess::GetGlobal()->local_state()));
    account_id_ = AccountId::FromUserEmail(kTestAccount);
    const std::string username_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id_);
    fake_user_manager()->AddUser(account_id_);
    fake_user_manager()->UserLoggedIn(account_id_, username_hash,
                                      /*browser_restart=*/false,
                                      /*is_child=*/false);
    CoreAccountInfo account_info;
    account_info.email = kTestAccount;
    account_info.gaia = "gaia";
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service_.SetSignedIn(signin::ConsentLevel::kSync, account_info);

    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    auto* prefs_ptr = prefs.get();
    profile_ = profile_manager_->CreateTestingProfile(
        kTestAccount, std::move(prefs), std::u16string(), /*avatar_id=*/0,
        TestingProfile::TestingFactories());
    fake_user_manager()->OnUserProfileCreated(account_id_, prefs_ptr);
    fake_desk_sync_service_ =
        std::make_unique<desks_storage::FakeDeskSyncService>(
            /*skip_engine_connection=*/true);
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    fake_device_info_sync_service_ =
        std::make_unique<syncer::FakeDeviceInfoSyncService>(
            /*skip_engine_connection=*/true);
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
  }

  void TearDown() override {
    auto* floating_workspace_service =
        FloatingWorkspaceServiceFactory::GetForProfile(profile());
    if (floating_workspace_service) {
      floating_workspace_service->ShutDownServicesAndObservers();
    }
    fake_user_manager()->OnUserProfileWillBeDestroyed(account_id_);
    profile_ = nullptr;
    profile_manager_ = nullptr;
    mock_desks_client_ = nullptr;
    ash_test_helper_.TearDown();
    chromeos::PowerManagerClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::TestSyncService test_sync_service_;
  std::unique_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  std::unique_ptr<syncer::FakeDeviceInfoSyncService>
      fake_device_info_sync_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
  AshTestHelper ash_test_helper_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<MockDesksClient> mock_desks_client_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  ash::SessionTerminationManager session_termination_manager_;

  raw_ptr<TestingProfile> profile_ = nullptr;
};

class FloatingWorkspaceServiceV1Test : public FloatingWorkspaceServiceTest {
 protected:
  FloatingWorkspaceServiceV1Test() = default;
  ~FloatingWorkspaceServiceV1Test() override = default;

  void SetUp() override {
    scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
    FloatingWorkspaceServiceTest::SetUp();
  }

  void TearDown() override {
    FloatingWorkspaceServiceTest::TearDown();
    scoped_feature_list().Reset();
  }
};

class FloatingWorkspaceServiceV2Test : public FloatingWorkspaceServiceTest {
 protected:
  FloatingWorkspaceServiceV2Test() = default;
  ~FloatingWorkspaceServiceV2Test() override = default;

  void SetUp() override {
    scoped_feature_list().InitWithFeatures(
        {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
    FloatingWorkspaceServiceTest::SetUp();
  }

  void TearDown() override {
    FloatingWorkspaceServiceTest::TearDown();
    scoped_feature_list().Reset();
  }
};

TEST_F(FloatingWorkspaceServiceV1Test, RestoreRemoteSession) {
  PopulateAppsCache();
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, more_recent_time);
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions;
  // This remote session has most recent timestamp and should be restored.
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, most_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetForeignSessionForTesting(foreign_sessions);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(
      kRemoteSessionOneName,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceV1Test, RestoreLocalSession) {
  PopulateAppsCache();
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, most_recent_time);
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetForeignSessionForTesting(foreign_sessions);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(
      kLocalSessionName,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceV1Test, RestoreRemoteSessionAfterUpdated) {
  PopulateAppsCache();
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, most_recent_time);
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetForeignSessionForTesting(foreign_sessions);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  // Simulate remote session update arrives 1 seconds after service
  // initialization.
  base::TimeDelta remote_session_update_arrival_time = base::Seconds(1);
  task_environment().FastForwardBy(remote_session_update_arrival_time);
  // Remote session got updated during the 3 second delay of dispatching task
  // and updated remote session is most recent.
  base::Time remote_session_updated_time = most_recent_time + base::Seconds(5);
  // Now previously less recent remote session becomes most recent
  // and should be restored.
  less_recent_remote_session->SetModifiedTime(remote_session_updated_time);

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get() -
      remote_session_update_arrival_time);
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(
      less_recent_remote_session->GetSessionName(),
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceV1Test, NoLocalSession) {
  PopulateAppsCache();
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service.SetForeignSessionForTesting(foreign_sessions);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  // Wait for kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin seconds.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get());

  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(
      most_recent_remote_session->GetSessionName(),
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceV1Test, NoRemoteSession) {
  PopulateAppsCache();

  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, least_recent_time);

  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr, /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  // Wait for kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin seconds.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get());

  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(
      kLocalSessionName,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceV1Test, NoSession) {
  PopulateAppsCache();

  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr, /*fake_device_info_sync_service*/ nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();

  // Wait for kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin seconds.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get());

  EXPECT_FALSE(test_floating_workspace_service.GetRestoredSession());
}

TEST_F(FloatingWorkspaceServiceV2Test, RestoreFloatingWorkspaceTemplate) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceV2Test, NoNetworkForFloatingWorkspaceTemplate) {
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().RunUntilIdle();
  EXPECT_TRUE(HasNotificationFor(kNotificationForNoNetworkConnection));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       NoNetworkForFloatingWorkspaceTemplateAfterLongDelay) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(HasNotificationFor(kNotificationForNoNetworkConnection));
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() -
      base::Milliseconds(1));
  CleanUpTestNetworkDevices();
  task_environment().RunUntilIdle();
  EXPECT_TRUE(HasNotificationFor(kNotificationForNoNetworkConnection));
}

TEST_F(
    FloatingWorkspaceServiceV2Test,
    NoNetworkForFloatingWorkspaceTemplateNotificationGoneAfterNetworkIsConnected) {
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  task_environment().RunUntilIdle();

  EXPECT_TRUE(HasNotificationFor(kNotificationForNoNetworkConnection));
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  task_environment().RunUntilIdle();
  floating_workspace_service->DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  EXPECT_FALSE(HasNotificationFor(kNotificationForNoNetworkConnection));
}

TEST_F(FloatingWorkspaceServiceV2Test,
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Disconnect from internet. Make sure no notification is sent since restore
  // happened already.
  CleanUpTestNetworkDevices();
  task_environment().RunUntilIdle();
  EXPECT_FALSE(HasNotificationFor(kNotificationForNoNetworkConnection));
  // Sanity check. Add network back and make sure notification is still gone.
  AddTestNetworkDevice();
  network_handler_test_helper()->ResetDevicesAndServices();
  network_handler_test_helper()->ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "Strength": 50, "AutoConnect": true, "WiFi.HiddenSSID":
            false})");
  floating_workspace_service->DefaultNetworkChanged(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  EXPECT_FALSE(HasNotificationFor(kNotificationForNoNetworkConnection));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       NoNetworkNotificationLogicWhenSyncIsInactiveAndOnceSyncIsActiveAgain) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  test_sync_service()->SetAllowedByEnterprisePolicy(false);
  ASSERT_FALSE(test_sync_service()->IsSyncFeatureEnabled());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(HasNotificationFor(kNotificationForNoNetworkConnection));
  test_sync_service()->SetAllowedByEnterprisePolicy(true);
  ASSERT_TRUE(test_sync_service()->IsSyncFeatureEnabled());
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForNoNetworkConnection));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateRestoreAfterTimeOut) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  // User clicks restore button on the notification.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kRestore),
      std::nullopt);
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateDiscardAfterTimeOut) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  // Download completes after timeout.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  // User clicks restore button on the notification.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kCancel),
      std::nullopt);
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
}

TEST_F(FloatingWorkspaceServiceV2Test, CanRecordTemplateLoadMetric) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test, CanRecordTemplateLaunchTimeout) {
  PopulateAppsCache();
  base::HistogramTester histogram_tester;

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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));

  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::
          kFloatingWorkspaceV2TemplateLaunchTimedOut,
      1u);
  histogram_tester.ExpectBucketCount(
      floating_workspace_metrics_util::
          kFloatingWorkspaceV2TemplateLaunchTimedOut,
      static_cast<int>(floating_workspace_metrics_util::
                           LaunchTemplateTimeoutType::kPassedWaitPeriod),
      1u);
}

TEST_F(FloatingWorkspaceServiceV2Test, CaptureFloatingWorkspaceTemplate) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceV2Test, CaptureSameFloatingWorkspaceTemplate) {
  // Upload should be skipped if two captured templates are the same.

  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  // Second captured template is the same as first, template should not be
  // updated, creation time is first template's creation time.
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            first_captured_template_creation_time);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       CaptureDifferentFloatingWorkspaceTemplate) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  // Second captured template has different restore data than first, template
  // should be updated, replacing the first one.
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            second_captured_template_creation_time);
}

TEST_F(FloatingWorkspaceServiceV2Test, PopulateFloatingWorkspaceTemplate) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      floating_workspace_service->GetFloatingWorkspaceTemplateEntries().size(),
      1u);
}

TEST_F(FloatingWorkspaceServiceV2Test,
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test,
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test, PerformGarbageCollectionOnStaleEntries) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateHasProgressStatus) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));

  // Wait for download to complete and check that the progress bar is gone.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateProgressStatusGoneAfterTimeOut) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));
  // Wait for timeout and check that the progress bar is gone.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateProgressStatusGoneAfterSyncError) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));
  // Send sync error to service.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kError);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateRestoreAfterTimeOutWithNewCapture) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Capture a new desk before user restores.
  const std::string captured_template_name =
      "floating_workspace_captured_template";
  const base::Time captured_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(captured_template_name,
                                            captured_creation_time);

  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(captured_floating_workspace_template));

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            captured_creation_time);

  // User clicks restore button on the notification and we should the entry
  // prior to the capture.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kRestore),
      std::nullopt);
  EXPECT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       RestoreWhenNoFloatingWorkspaceTemplateIsAvailable) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
}

TEST_F(FloatingWorkspaceServiceV2Test, CanRecordTemplateNotFoundMetric) {
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(mock_desks_client()->restored_desk_template());
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateNotFound,
      1u);
}

TEST_F(FloatingWorkspaceServiceV2Test, CanRecordFloatingWorkspaceV2InitMetric) {
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2Initialized, 1u);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       CaptureButDontUploadIfNoUserActionAfterkUpToDate) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Idle for a while.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_FALSE(
      floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
}

TEST_F(FloatingWorkspaceServiceV2Test,
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test,
       CaptureButDontUploadIfNoUserActionAfterLastUpload) {
  // Upload should be executed if two captured templates are the different.

  PopulateAppsCache();
  // Idle for a while.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                  ->template_name() != base::UTF8ToUTF16(template_name2));
}

TEST_F(FloatingWorkspaceServiceV2Test, CaptureImmediatelyAfterRestore) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
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

TEST_F(FloatingWorkspaceServiceV2Test,
       CaptureFloatingWorkspaceTemplateOnSystemTrayVisible) {
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  mock_desks_client()->SetCapturedDeskTemplate(
      std::move(floating_workspace_template));
  ash::Shell::Get()->system_tray_notifier()->NotifySystemTrayBubbleShown();
  ASSERT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(floating_workspace_service->GetLatestFloatingWorkspaceTemplate()
                ->created_time(),
            creation_time);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       CaptureFloatingWorkspaceTemplateOnLockScreen) {
  SessionControllerClientImpl client;
  client.Init();
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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

TEST_F(FloatingWorkspaceServiceV2Test,
       RestoreFloatingWorkspaceTemplateAfterWakingUpFromSleep) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  // Wake device up. Go through normal restore flow.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
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

TEST_F(FloatingWorkspaceServiceV2Test,
       RestoreFloatingWorkspaceTemplateTimeoutAfterWakingFromSleep) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  // Wake device up. Go through normal restore flow.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);

  task_environment().FastForwardBy(base::Seconds(3));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get());
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
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
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  // User clicks restore button on the notification.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kRestore),
      std::nullopt);
  ASSERT_TRUE(mock_desks_client()->restored_template_uuid().is_valid());
  EXPECT_EQ(mock_desks_client()->restored_template_uuid(),
            new_floating_workspace_template->uuid());
}

TEST_F(
    FloatingWorkspaceServiceV2Test,
    RestoreFloatingWorkspaceTemplateNoTimeoutAfterWakingFromSleepWithNoNewEntry) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Send device to sleep.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  // Wake device up. Go through normal restore flow.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForRestoreAfterError));
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       FloatingWorkspaceTemplateRestoreAfterTimeOutNotificationFromSleep) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Send device to sleep.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  // Wake device up. Go through normal restore flow.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
  task_environment().FastForwardBy(base::Seconds(3));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kWaitingForUpdates);
  test_sync_service()->FireStateChanged();
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
}

TEST_F(FloatingWorkspaceServiceV2Test,
       DoNotShowTimeOutNotificationAfterRestoreTimeoutFromSuspend) {
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(mock_desks_client()->restored_desk_template());
  EXPECT_EQ(mock_desks_client()->restored_desk_template()->template_name(),
            base::UTF8ToUTF16(template_name));
  // Send device to sleep.
  power_manager_client()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  // Wake device up. Go through normal restore flow.
  power_manager_client()->SendSuspendDone();
  floating_workspace_service->OnLockStateChanged(/*locked=*/false);
  task_environment().FastForwardBy(base::Seconds(3));
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_FALSE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
}

TEST_F(FloatingWorkspaceServiceV2Test, AutoSignoutWithDeviceInfo) {
  PopulateAppsCache();
  base::RunLoop loop;
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(ash_test_helper()
                ->test_session_controller_client()
                ->request_sign_out_count(),
            1);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       AutoSignoutDontTriggerWithSameDeviceInfoGuid) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() + base::Seconds(10)));
  fake_device_info_sync_service()->GetDeviceInfoTracker()->SetLocalCacheGuid(
      "guid1");
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(ash_test_helper()
                ->test_session_controller_client()
                ->request_sign_out_count(),
            0);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       AutoSignoutDontTriggerWithOldDeviceInfo) {
  PopulateAppsCache();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  fake_device_info_sync_service()->GetDeviceInfoTracker()->Add(
      CreateFakeDeviceInfo("guid1", "device1",
                           base::Time::Now() - base::Seconds(10)));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::DEVICE_INFO},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(ash_test_helper()
                ->test_session_controller_client()
                ->request_sign_out_count(),
            0);
}

TEST_F(FloatingWorkspaceServiceV2Test, AutoSignoutWithWorkspaceDesk) {
  // Upload should be executed if two captured templates are the different.
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name,
          base::Time::Now() +
              ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds
                  .Get() +
              base::Seconds(1)),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(ash_test_helper()
                ->test_session_controller_client()
                ->request_sign_out_count(),
            1);
}

TEST_F(FloatingWorkspaceServiceV2Test,
       AutoSignoutDontTriggerWithStaleWorkspaceDesk) {
  // Upload should be executed if two captured templates are the different.
  PopulateAppsCache();
  CreateFloatingWorkspaceServiceForTesting(profile());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  base::RunLoop loop;
  fake_desk_sync_service()->GetDeskModel()->AddOrUpdateEntry(
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name,
          base::Time::Now() -
              ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds
                  .Get() -
              base::Seconds(1)),
      base::BindLambdaForTesting(
          [&](desks_storage::DeskModel::AddOrUpdateEntryStatus status,
              std::unique_ptr<ash::DeskTemplate> new_entry) {
            EXPECT_EQ(desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk,
                      status);
            loop.Quit();
          }));
  loop.Run();
  test_sync_service()->SetDownloadStatusFor(
      {syncer::DataType::WORKSPACE_DESK},
      syncer::SyncService::DataTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_EQ(ash_test_helper()
                ->test_session_controller_client()
                ->request_sign_out_count(),
            0);
}

class FloatingWorkspaceServiceMultiUserTest
    : public FloatingWorkspaceServiceV2Test {
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

  syncer::FakeDeviceInfoSyncService* fake_device_info_sync_service2() {
    return fake_device_info_sync_service2_.get();
  }

  void PopulateAppsCache2() {
    desks_storage::desk_test_util::PopulateFloatingWorkspaceAppRegistryCache(
        account_id2_, cache2_.get());
    task_environment().RunUntilIdle();
  }

  void SetUp() override {
    FloatingWorkspaceServiceV2Test::SetUp();
    EXPECT_TRUE(temp_dir2_.CreateUniqueTempDir());
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    profile2_ = profile_manager()->CreateTestingProfile(
        kTestAccount2, std::move(prefs), std::u16string(),
        /*avatar_id=*/0, TestingProfile::TestingFactories());

    account_id2_ = AccountId::FromUserEmail(kTestAccount2);
    const std::string username_hash2 =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id2_);
    fake_user_manager()->AddUser(account_id2_);
    fake_user_manager()->UserLoggedIn(account_id2_, username_hash2,
                                      /*browser_restart=*/false,
                                      /*is_child=*/false);
    CoreAccountInfo account_info;
    account_info.email = kTestAccount2;
    account_info.gaia = "gaia2";
    account_info.account_id = CoreAccountId::FromGaiaId(account_info.gaia);
    test_sync_service()->SetSignedIn(signin::ConsentLevel::kSync, account_info);
    fake_desk_sync_service2_ =
        std::make_unique<desks_storage::FakeDeskSyncService>(
            /*skip_engine_connection=*/true);
    test_sync_service2_ = std::make_unique<syncer::TestSyncService>();

    display_service2_ =
        std::make_unique<NotificationDisplayServiceTester>(profile2_.get());
    cache2_ = std::make_unique<apps::AppRegistryCache>();
    fake_device_info_sync_service2_ =
        std::make_unique<syncer::FakeDeviceInfoSyncService>(
            /*skip_engine_connection=*/true);
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
    FloatingWorkspaceServiceV2Test::TearDown();
  }

 private:
  std::unique_ptr<syncer::TestSyncService> test_sync_service2_;
  std::unique_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service2_;
  base::ScopedTempDir temp_dir2_;
  AccountId account_id2_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service2_;
  std::unique_ptr<apps::AppRegistryCache> cache2_;
  std::unique_ptr<syncer::FakeDeviceInfoSyncService>
      fake_device_info_sync_service2_;
  raw_ptr<TestingProfile> profile2_ = nullptr;
};

TEST_F(FloatingWorkspaceServiceMultiUserTest, TwoUserLoggedInAndCaptureStops) {
  PopulateAppsCache();
  PopulateAppsCache2();
  CreateFloatingWorkspaceServiceForTesting(profile());
  CreateFloatingWorkspaceServiceForTesting(profile2());
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  auto* floating_workspace_service2 =
      FloatingWorkspaceServiceFactory::GetForProfile(profile2());
  floating_workspace_service2->Init(test_sync_service2(),
                                    fake_desk_sync_service2(),
                                    fake_device_info_sync_service2());
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
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
  auto* floating_workspace_service =
      FloatingWorkspaceServiceFactory::GetForProfile(profile());
  floating_workspace_service->Init(test_sync_service(),
                                   fake_desk_sync_service(),
                                   fake_device_info_sync_service());
  auto* floating_workspace_service2 =
      FloatingWorkspaceServiceFactory::GetForProfile(profile2());
  floating_workspace_service2->Init(test_sync_service2(),
                                    fake_desk_sync_service2(),
                                    fake_device_info_sync_service2());

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
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  EXPECT_TRUE(floating_workspace_service->GetLatestFloatingWorkspaceTemplate());
  EXPECT_FALSE(
      floating_workspace_service2->GetLatestFloatingWorkspaceTemplate());
}
}  // namespace ash::floating_workspace
