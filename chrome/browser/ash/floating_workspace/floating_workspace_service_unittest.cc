// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char local_session_name[] = "local_session";
constexpr char remote_session_name[] = "remote_session";
const base::Time more_recent_time = base::Time::FromDoubleT(10);
const base::Time less_recent_time = base::Time::FromDoubleT(5);
std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const base::Time& session_time) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  session->session_name = session_name;
  session->modified_time = session_time;
  return session;
}
}  // namespace

namespace ash {
class TestFloatingWorkSpaceService : public ash::FloatingWorkspaceService {
 public:
  explicit TestFloatingWorkSpaceService(TestingProfile* profile)
      : ash::FloatingWorkspaceService(profile) {}
  void RestoreLocalSessionWindows() override {
    restored_session_ = GetLocalSession();
  }
  void RestoreForeignSessionWindows(
      const sync_sessions::SyncedSession* session) override {
    restored_session_ = session;
  }
  const sync_sessions::SyncedSession* GetLocalSession() override {
    return local_session_;
  }
  const sync_sessions::SyncedSession* GetMostRecentlyUsedRemoteSession()
      override {
    return most_recently_used_remote_session_;
  }
  const sync_sessions::SyncedSession* GetRestoredSession() {
    return restored_session_;
  }
  void SetLocalSessionForTesting(const sync_sessions::SyncedSession* session) {
    local_session_ = session;
  }
  void SetMostRecentlyUsedRemoteSession(
      const sync_sessions::SyncedSession* session) {
    most_recently_used_remote_session_ = session;
  }

 private:
  const sync_sessions::SyncedSession* restored_session_ = nullptr;
  const sync_sessions::SyncedSession* local_session_ = nullptr;
  const sync_sessions::SyncedSession* most_recently_used_remote_session_ =
      nullptr;
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
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  // Remote session is more recent at the beginning.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, less_recent_time);
  std::unique_ptr<sync_sessions::SyncedSession> remote_session =
      CreateNewSession(remote_session_name, more_recent_time);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(
      remote_session.get());
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(remote_session_name,
            test_floating_workspace_service.GetRestoredSession()->session_name);
}

TEST_F(FloatingWorkspaceServiceTest, RestoreLocalSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  // Local session is more recent.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, more_recent_time);
  std::unique_ptr<sync_sessions::SyncedSession> remote_session =
      CreateNewSession(remote_session_name, less_recent_time);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(
      remote_session.get());
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(local_session_name,
            test_floating_workspace_service.GetRestoredSession()->session_name);
}

TEST_F(FloatingWorkspaceServiceTest, RestoreRemoteSessionAfterUpdated) {
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  // Local session is more recent at the beginning
  // but updated remote session within 3 seconds is more recent.
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, more_recent_time);
  std::unique_ptr<sync_sessions::SyncedSession> remote_session =
      CreateNewSession(remote_session_name, less_recent_time);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(
      remote_session.get());
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop first_run_loop;
  base::TimeDelta first_run_loop_delay_time = base::Seconds(1);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, first_run_loop.QuitClosure(), first_run_loop_delay_time);
  first_run_loop.Run();
  // Remote session got updated during the 3 second delay of dispatching task
  // and updated remote session is most recent.
  base::Time remote_session_updated_time = more_recent_time + base::Seconds(5);
  remote_session =
      CreateNewSession(remote_session_name, remote_session_updated_time);
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(
      remote_session.get());
  base::RunLoop second_run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, second_run_loop.QuitClosure(),
      GetMaxRestoreTime() - first_run_loop_delay_time);
  second_run_loop.Run();
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(remote_session_name,
            test_floating_workspace_service.GetRestoredSession()->session_name);
}

TEST_F(FloatingWorkspaceServiceTest, NoLocalSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  std::unique_ptr<sync_sessions::SyncedSession> remote_session =
      CreateNewSession(remote_session_name, less_recent_time);
  test_floating_workspace_service.SetLocalSessionForTesting(nullptr);
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(
      remote_session.get());
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(remote_session_name,
            test_floating_workspace_service.GetRestoredSession()->session_name);
}
TEST_F(FloatingWorkspaceServiceTest, NoRemoteSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  std::unique_ptr<sync_sessions::SyncedSession> local_session =
      CreateNewSession(local_session_name, less_recent_time);
  test_floating_workspace_service.SetLocalSessionForTesting(
      local_session.get());
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(nullptr);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_TRUE(test_floating_workspace_service.GetRestoredSession());
  EXPECT_EQ(local_session_name,
            test_floating_workspace_service.GetRestoredSession()->session_name);
}

TEST_F(FloatingWorkspaceServiceTest, NoSession) {
  TestFloatingWorkSpaceService test_floating_workspace_service(profile());
  test_floating_workspace_service.SetLocalSessionForTesting(nullptr);
  test_floating_workspace_service.SetMostRecentlyUsedRemoteSession(nullptr);
  test_floating_workspace_service
      .RestoreBrowserWindowsFromMostRecentlyUsedDevice();
  // Wait for 3 seconds which is kMaxTimeAvailableForRestoreAfterLogin.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), GetMaxRestoreTime());
  run_loop.Run();
  EXPECT_FALSE(test_floating_workspace_service.GetRestoredSession());
}
}  // namespace ash
