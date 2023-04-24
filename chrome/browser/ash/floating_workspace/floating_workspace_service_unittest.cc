// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char local_session_name[] = "local_session";
constexpr char remote_session_1_name[] = "remote_session_1";
constexpr char remote_session_2_name[] = "remote_session_2";
const base::Time most_recent_time = base::Time::FromDoubleT(15);
const base::Time more_recent_time = base::Time::FromDoubleT(10);
const base::Time least_recent_time = base::Time::FromDoubleT(5);
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
    std::string name) {
  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          base::Uuid::ParseCaseInsensitive(
              "c098bdcf-5803-484b-9bfd-d3a9a4b497ab"),
          ash::DeskTemplateSource::kUser, name, base::Time::Now(),
          DeskTemplateType::kFloatingWorkspace);
  std::unique_ptr<app_restore::RestoreData> restore_data =
      CreateRestoreData(std::vector<int>(10, 1));
  desk_template->set_desk_restore_data(std::move(restore_data));
  return desk_template;
}

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
  raw_ptr<sync_sessions::SyncedSession, ExperimentalAsh> local_session_ =
      nullptr;
};

}  // namespace

class TestFloatingWorkSpaceService : public FloatingWorkspaceService {
 public:
  explicit TestFloatingWorkSpaceService(TestingProfile* profile,
                                        TestFloatingWorkspaceVersion version)
      : FloatingWorkspaceService(profile) {
    InitForTest(version);
    mock_open_tabs_ = std::make_unique<MockOpenTabsUIDelegate>();
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

  const DeskTemplate* GetRestoredFloatingWorkspaceTemplate() {
    return restored_floating_workspace_template_;
  }

 private:
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return mock_open_tabs_.get();
  }

  void LaunchFloatingWorkspaceTemplate(
      const DeskTemplate* desk_template) override {
    restored_floating_workspace_template_ = desk_template;
  }

  const sync_sessions::SyncedSession* restored_session_ = nullptr;
  raw_ptr<const DeskTemplate, ExperimentalAsh>
      restored_floating_workspace_template_ = nullptr;
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

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    profile_builder.SetProfileName("user.test@gmail.com");
    profile_builder.SetPath(
        temp_dir.GetPath().AppendASCII("TestFloatingWorkspace"));
    profile_ = profile_builder.Build();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(FloatingWorkspaceServiceTest, RestoreRemoteSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, more_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  // This remote session has most recent timestamp and should be restored.
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(remote_session_1_name, most_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(remote_session_2_name, least_recent_time);
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
      remote_session_1_name,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceTest, RestoreLocalSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, most_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(remote_session_1_name, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(remote_session_2_name, least_recent_time);
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
      local_session_name,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceTest, RestoreRemoteSessionAfterUpdated) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  // Local session has most recent timestamp and should be restored.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, most_recent_time);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(remote_session_1_name, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(remote_session_2_name, least_recent_time);
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
}

TEST_F(FloatingWorkspaceServiceTest, NoLocalSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  std::vector<const sync_sessions::SyncedSession*> foreign_sessions;
  const std::unique_ptr<sync_sessions::SyncedSession>
      most_recent_remote_session =
          CreateNewSession(remote_session_1_name, more_recent_time);
  const std::unique_ptr<sync_sessions::SyncedSession>
      less_recent_remote_session =
          CreateNewSession(remote_session_2_name, least_recent_time);
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
}

TEST_F(FloatingWorkspaceServiceTest, NoRemoteSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, least_recent_time);
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
      local_session_name,
      test_floating_workspace_service.GetRestoredSession()->GetSessionName());
}

TEST_F(FloatingWorkspaceServiceTest, NoSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV1Enabled);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin seconds.
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceMaxTimeAvailableForRestoreAfterLogin
          .Get());

  EXPECT_FALSE(test_floating_workspace_service.GetRestoredSession());
}

TEST_F(FloatingWorkspaceServiceTest, RestoreFloatingWorkspaceTemplate) {
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV2Enabled);
  std::vector<const DeskTemplate*> desk_template_entries;
  const std::string template_name = "floating_workspace_template";
  std::unique_ptr<const DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name);
  desk_template_entries.push_back(floating_workspace_template.get());
  test_floating_workspace_service_v2.EntriesAddedOrUpdatedRemotely(
      desk_template_entries);
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
}

TEST_F(FloatingWorkspaceServiceTest, FloatingWorkspaceTemplateTimeOut) {
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV2Enabled);
  std::vector<const DeskTemplate*> desk_template_entries;
  const std::string template_name = "floating_workspace_template";
  std::unique_ptr<const DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name);
  desk_template_entries.push_back(floating_workspace_template.get());
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  test_floating_workspace_service_v2.EntriesAddedOrUpdatedRemotely(
      desk_template_entries);
  EXPECT_FALSE(test_floating_workspace_service_v2
                   .GetRestoredFloatingWorkspaceTemplate());
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateLoadMetric) {
  base::HistogramTester histogram_tester;
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV2Enabled);
  std::vector<const DeskTemplate*> desk_template_entries;
  const std::string template_name = "floating_workspace_template";
  std::unique_ptr<const DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name);
  desk_template_entries.push_back(floating_workspace_template.get());
  test_floating_workspace_service_v2.EntriesAddedOrUpdatedRemotely(
      desk_template_entries);
  EXPECT_TRUE(test_floating_workspace_service_v2
                  .GetRestoredFloatingWorkspaceTemplate());
  EXPECT_EQ(
      test_floating_workspace_service_v2.GetRestoredFloatingWorkspaceTemplate()
          ->template_name(),
      base::UTF8ToUTF16(template_name));
  histogram_tester.ExpectTotalCount(
      floating_workspace_metrics_util::kFloatingWorkspaceV2TemplateLoadTime,
      1u);
}

TEST_F(FloatingWorkspaceServiceTest, CanRecordTemplateLaunchTimeout) {
  base::HistogramTester histogram_tester;
  TestFloatingWorkSpaceService test_floating_workspace_service_v2(
      profile(), TestFloatingWorkspaceVersion::kFloatingWorkspaceV2Enabled);
  std::vector<const DeskTemplate*> desk_template_entries;
  const std::string template_name = "floating_workspace_template";
  std::unique_ptr<const DeskTemplate> floating_workspace_template =
      MakeTestFloatingWorkspaceDeskTemplate(template_name);
  desk_template_entries.push_back(floating_workspace_template.get());
  task_environment().FastForwardBy(
      ash::features::kFloatingWorkspaceV2MaxTimeAvailableForRestoreAfterLogin
          .Get() +
      base::Seconds(1));
  test_floating_workspace_service_v2.EntriesAddedOrUpdatedRemotely(
      desk_template_entries);
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
}

}  // namespace ash
