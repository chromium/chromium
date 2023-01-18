// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "base/files/scoped_temp_dir.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
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
  sync_sessions::SyncedSession* local_session_ = nullptr;
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

 private:
  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate() override {
    return mock_open_tabs_.get();
  }
  const sync_sessions::SyncedSession* restored_session_ = nullptr;
  std::unique_ptr<MockOpenTabsUIDelegate> mock_open_tabs_;
};

class FloatingWorkspaceServiceTest : public testing::Test {
 public:
  FloatingWorkspaceServiceTest() = default;

  ~FloatingWorkspaceServiceTest() override = default;

  TestingProfile* profile() const { return profile_.get(); }

  const base::TimeDelta GetMaxRestoreTime() { return max_restore_time_; }

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
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  const base::TimeDelta max_restore_time_ = base::Seconds(3);
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop first_run_loop;
  base::TimeDelta first_run_loop_delay_time = base::Seconds(1);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, first_run_loop.QuitClosure(), first_run_loop_delay_time);
  first_run_loop.Run();
  // Remote session got updated during the 3 second delay of dispatching task
  // and updated remote session is most recent.
  base::Time remote_session_updated_time = most_recent_time + base::Seconds(5);
  std::vector<const sync_sessions::SyncedSession*> updated_foreign_sessions;
  // Now previously less recent remote session becomes most recent
  // and should be restored.
  less_recent_remote_session->SetModifiedTime(remote_session_updated_time);
  base::RunLoop second_run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, second_run_loop.QuitClosure(),
      GetMaxRestoreTime() - first_run_loop_delay_time);
  second_run_loop.Run();
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
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
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_FALSE(test_floating_workspace_service.GetRestoredSession());
}
}  // namespace ash
