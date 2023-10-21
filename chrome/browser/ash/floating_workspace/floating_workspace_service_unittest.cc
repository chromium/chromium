// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include <memory>
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
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
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/fake_desk_sync_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::floating_workspace {

namespace {

constexpr char kLocalSessionName[] = "local_session";
constexpr char kRemoteSessionOneName[] = "remote_session_1";
constexpr char kRemoteSession2Name[] = "remote_session_2";
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
          absl::make_optional<int32_t>(activation_index_counter++);

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
  MOCK_METHOD((absl::optional<DesksClient::DeskActionError>),
              RemoveDesk,
              (const base::Uuid& desk_uuid, ash::DeskCloseType close_type),
              (override));

  void CaptureActiveDesk(CaptureActiveDeskAndSaveTemplateCallback callback,
                         ash::DeskTemplateType template_type) override {
    std::move(callback).Run(absl::nullopt, captured_desk_template_->Clone());
  }

  void SetCapturedDeskTemplate(
      std::unique_ptr<const DeskTemplate> captured_template) {
    captured_desk_template_ = std::move(captured_template);
  }

 private:
  std::unique_ptr<const DeskTemplate> captured_desk_template_;
};

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() = default;

  bool GetAllForeignSessions(
      std::vector<const sync_sessions::SyncedSession*>* sessions) override {
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
      std::vector<const sync_sessions::SyncedSession*> foreign_sessions) {
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

  MOCK_METHOD2(GetForeignSession,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionWindow*>* windows));

  MOCK_METHOD2(GetForeignSessionTabs,
               bool(const std::string& tag,
                    std::vector<const sessions::SessionTab*>* tabs));

 private:
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions_;
  raw_ptr<sync_sessions::SyncedSession, DanglingUntriaged | ExperimentalAsh>
      local_session_ = nullptr;
};

}  // namespace

class TestFloatingWorkSpaceService : public FloatingWorkspaceService {
 public:
  explicit TestFloatingWorkSpaceService(
      TestingProfile* profile,
      raw_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service,
      raw_ptr<syncer::TestSyncService> mock_sync_service,
      floating_workspace_util::FloatingWorkspaceVersion version)
      : FloatingWorkspaceService(profile, version) {
    is_testing_ = true;
    Init(mock_sync_service, fake_desk_sync_service);
    mock_open_tabs_ = std::make_unique<MockOpenTabsUIDelegate>();
    mock_desks_client_ = std::make_unique<MockDesksClient>();
  }

  void RestoreLocalSessionWindows() override {
    mock_open_tabs_->GetLocalSession(&restored_session_);
  }

  void RestoreForeignSessionWindows(
      const sync_sessions::SyncedSession* session) override {
    restored_session_ = session;
  }

  const sync_sessions::SyncedSession* GetRestoredSession() {
    return restored_session_;
  }

  void SetLocalSessionForTesting(sync_sessions::SyncedSession* session) {
    mock_open_tabs_->SetLocalSessionForTesting(session);
  }

  void SetForeignSessionForTesting(
      std::vector<const sync_sessions::SyncedSession*> foreign_sessions) {
    mock_open_tabs_->SetForeignSessionsForTesting(foreign_sessions);
  }

  MockDesksClient* GetMockDesksClient() { return mock_desks_client_.get(); }

  const DeskTemplate* GetRestoredFloatingWorkspaceTemplate() {
    return restored_floating_workspace_template_;
  }

  DeskTemplate* GetUploadedFloatingWorkspaceTemplate() {
    return uploaded_desk_template_;
  }

 private:
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return mock_open_tabs_.get();
  }

  void LaunchFloatingWorkspaceTemplate(
      const DeskTemplate* desk_template) override {
    restored_floating_workspace_template_ = desk_template;
  }
  void UploadFloatingWorkspaceTemplateToDeskModel(
      std::unique_ptr<DeskTemplate> desk_template) override {
    uploaded_desk_template_ = desk_template.get();
    previously_captured_desk_template_ = std::move(desk_template);
  }

  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #addr-of
  RAW_PTR_EXCLUSION const sync_sessions::SyncedSession* restored_session_ =
      nullptr;
  raw_ptr<const DeskTemplate, DanglingUntriaged | ExperimentalAsh>
      restored_floating_workspace_template_ = nullptr;
  raw_ptr<DeskTemplate, ExperimentalAsh> uploaded_desk_template_ = nullptr;
  std::unique_ptr<MockOpenTabsUIDelegate> mock_open_tabs_;
  std::unique_ptr<MockDesksClient> mock_desks_client_;
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

  syncer::TestSyncService* test_sync_service() {
    return test_sync_service_.get();
  }

  ui::UserActivityDetector* user_activity_detector() {
    return user_activity_detector_.get();
  }
  bool HasNotificationFor(const std::string& id) {
    absl::optional<message_center::Notification> notification =
        display_service()->GetNotification(id);
    return notification.has_value();
  }

  void AddTestNetworkDevice() {
    network_handler_test_helper_->AddDefaultProfiles();
  }

  void CleanUpTestNetworkDevices() {
    network_handler_test_helper_->ClearDevices();
    network_handler_test_helper_->ClearServices();
  }

  apps::AppRegistryCache* cache() { return cache_.get(); }

  AccountId& account_id() { return account_id_; }

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

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    profile_builder.SetProfileName("user.test@gmail.com");
    profile_builder.SetPath(
        temp_dir.GetPath().AppendASCII("TestFloatingWorkspace"));
    profile_ = profile_builder.Build();
    fake_desk_sync_service_ =
        std::make_unique<desks_storage::FakeDeskSyncService>(
            /*skip_engine_connection=*/true);
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    AddTestNetworkDevice();
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    user_activity_detector_ = std::make_unique<ui::UserActivityDetector>();
    user_activity_detector_->set_last_activity_time_for_test(
        base::TimeTicks::Now());
    cache_ = std::make_unique<apps::AppRegistryCache>();
    account_id_ = multi_user_util::GetAccountIdFromProfile(profile_.get());
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             cache_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<desks_storage::FakeDeskSyncService> fake_desk_sync_service_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
  std::unique_ptr<apps::AppRegistryCache> cache_;
  AccountId account_id_;
};

TEST_F(FloatingWorkspaceServiceTest, RestoreRemoteSession) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, more_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  // This remote session has most recent timestamp and should be restored.
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, most_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());

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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, RestoreLocalSession) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, most_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());

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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, RestoreRemoteSessionAfterUpdated) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, most_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());

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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, NoLocalSession) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(kRemoteSessionOneName, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(kRemoteSession2Name, least_recent_time);
  foreign_sessions.push_back(less_recent_remote_session.get());
  foreign_sessions.push_back(most_recent_remote_session.get());
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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, NoRemoteSession) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(kLocalSessionName, least_recent_time);
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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, NoSession) {
  scoped_feature_list().InitWithFeatures({features::kFloatingWorkspace}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), /*fake_desk_sync_service=*/nullptr,
      /*mock_sync_service=*/nullptr,
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin seconds.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get());

  EXPECT_FALSE(test_floating_workspace_service.GetRestoredSession());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, RestoreFloatingWorkspaceTemplate) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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

  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, NoNetworkForFloatingWorkspaceTemplate) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  CleanUpTestNetworkDevices();
  const std::string template_name = "floating_workspace_template";
  base::RunLoop loop;

  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  EXPECT_TRUE(HasNotificationFor(kNotificationForNoNetworkConnection));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateRestoreAfterTimeOut) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  // User clicks restore button on the notification.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kRestore),
      absl::nullopt);
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateDiscardAfterTimeOut) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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

  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  // Download completes after timeout.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_TRUE(HasNotificationFor(kNotificationForRestoreAfterError));
  // User clicks restore button on the notification.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kCancel),
      absl::nullopt);
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateLoadMetric) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateLoadTime,
      1u);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateLaunchTimeout) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));

  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
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
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CaptureFloatingWorkspaceTemplate) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  const std::string template_name = "floating_workspace_captured_template";
  const base::Time creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name, creation_time);
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(std::move(floating_workspace_template));
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetUploadedFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetUploadedFloatingWorkspaceTemplate()
          ->created_time(),
      creation_time);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CaptureSameFloatingWorkspaceTemplate) {
  // Upload should be skipped if two captured templates are the same.
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
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
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
          std::move(second_captured_floating_workspace_template));
  // Fast forward by capture interval capture a second time.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetUploadedFloatingWorkspaceTemplate());
  // Second captured template is the same as first, template should not be
  // updated, creation time is first template's creation time.
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetUploadedFloatingWorkspaceTemplate()
          ->created_time(),
      first_captured_template_creation_time);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureDifferentFloatingWorkspaceTemplate) {
  // Upload should be executed if two captured templates are the different.
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
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
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
          std::move(second_captured_floating_workspace_template));
  // Fast forward by capture interval capture a second time.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetUploadedFloatingWorkspaceTemplate());
  // Second captured template has different restore data than first, template
  // should be updated, replacing the first one.
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetUploadedFloatingWorkspaceTemplate()
          ->created_time(),
      second_captured_template_creation_time);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, PopulateFloatingWorkspaceTemplate) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetFloatingWorkspaceTemplateEntries()
          .size(),
      1u);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       PopulateFloatingWorkspaceTemplateWithUpdates) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetFloatingWorkspaceTemplateEntries()
          .size(),
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
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetFloatingWorkspaceTemplateEntries()
          .size(),
      1u);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_EQ(test_floating_workspace_service_v2
                .GetFloatingWorkspaceTemplateEntries()[0]
                ->uuid(),
            template_2_uuid);

  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       DoNotPerformGarbageCollectionOnSingleEntryBeyondThreshold) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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

  task_environment().FastForwardBy(base::Days(31));
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  ASSERT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(fws_name));

  EXPECT_EQ(
      1ul, fake_desk_sync_service()->GetDeskModel()->GetAllEntryUuids().size());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, PerformGarbageCollectionOnStaleEntries) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  task_environment().FastForwardBy(base::Days(31));
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();

  ASSERT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(fws_two_name));

  EXPECT_EQ(
      1ul, fake_desk_sync_service()->GetDeskModel()->GetAllEntryUuids().size());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateHasProgressStatus) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));

  // Wait for download to complete and check that the progress bar is gone.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));

  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateProgressStatusGoneAfterTimeOut) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));
  // Wait for timeout and check that the progress bar is gone.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateProgressStatusGoneAfterSyncError) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(HasNotificationFor(kNotificationForProgressStatus));
  // Send sync error to service.
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kError);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(HasNotificationFor(kNotificationForProgressStatus));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       FloatingWorkspaceTemplateRestoreAfterTimeOutWithNewCapture) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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

  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  EXPECT_TRUE(HasNotificationFor(kNotificationForSyncErrorOrTimeOut));

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
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

  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
          std::move(captured_floating_workspace_template));

  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetUploadedFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetUploadedFloatingWorkspaceTemplate()
          ->created_time(),
      captured_creation_time);

  // User clicks restore button on the notification and we should the entry
  // prior to the capture.
  display_service()->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kNotificationForRestoreAfterError,
      static_cast<int>(RestoreFromErrorNotificationButtonIndex::kRestore),
      absl::nullopt);
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       RestoreWhenNoFloatingWorkspaceTemplateIsAvailable) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateNotFoundMetric) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateNotFound,
      1u);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordFloatingWorkspaceV2InitMetric) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  base::HistogramTester histogram_tester;
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);

  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2Initialized, 1u);
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       CaptureButDontUploadIfNoUserActionAfterkUpToDate) {
  // Upload should be executed if two captured templates are the different.
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
  PopulateAppsCache();
  user_activity_detector()->set_last_activity_time_for_test(
      base::TimeTicks::Now());
  // Idle for a while.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);
  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  const std::string template_name = "floating_workspace_captured_template";
  const base::Time first_captured_template_creation_time = base::Time::Now();
  std::unique_ptr<DeskTemplate> first_captured_floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(
          template_name, first_captured_template_creation_time);
  test_floating_workspace_service_v2.GetMockDesksClient()
      ->SetCapturedDeskTemplate(
          std::move(first_captured_floating_workspace_template));
  // Trigger the first capture task.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2PeriodicJobIntervalInSeconds.Get() +
      base::Seconds(1));

  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetUploadedFloatingWorkspaceTemplate());
  scoped_feature_list().Reset();
}

TEST_F(FloatingWorkspaceServiceTest,
       WaitForAppCacheBeforeRestoringFloatingWorkspaceTemplate) {
  scoped_feature_list().InitWithFeatures(
      {features::kFloatingWorkspaceV2, features::kDeskTemplateSync}, {});
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
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), fake_desk_sync_service(), test_sync_service(),
      floating_workspace_util::FloatingWorkspaceVersion::
          kFloatingWorkspaceV2Enabled);
  wrapper.AddAppRegistryCache(account_id(), cache());

  test_sync_service()->SetDownloadStatusFor(
      {syncer::ModelType::WORKSPACE_DESK},
      syncer::SyncService::ModelTypeDownloadStatus::kUpToDate);
  test_sync_service()->FireStateChanged();
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
  PopulateAppsCache();
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  scoped_feature_list().Reset();
}

}  // namespace ash::floating_workspace
