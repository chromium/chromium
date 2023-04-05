// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_base_test_helper.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

SessionServiceBaseTestHelper::SessionServiceBaseTestHelper(
    SessionServiceBase* base) {
  service_ = base;
}

void SessionServiceBaseTestHelper::SaveNow() {
  return service_->GetCommandStorageManagerForTest()->Save();
}

void SessionServiceBaseTestHelper::PrepareTabInWindow(SessionID window_id,
                                                      SessionID tab_id,
                                                      int visual_index,
                                                      bool select) {
  service_->SetTabWindow(window_id, tab_id);
  service_->SetTabIndexInWindow(window_id, tab_id, visual_index);
  if (select)
    service_->SetSelectedTabInWindow(window_id, visual_index);
}

void SessionServiceBaseTestHelper::SetTabExtensionAppID(
    SessionID window_id,
    SessionID tab_id,
    const std::string& extension_app_id) {
  service_->SetTabExtensionAppID(window_id, tab_id, extension_app_id);
}

void SessionServiceBaseTestHelper::SetTabUserAgentOverride(
    SessionID window_id,
    SessionID tab_id,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  service_->SetTabUserAgentOverride(window_id, tab_id, user_agent_override);
}

// Be sure and null out service to force closing the file.
void SessionServiceBaseTestHelper::ReadWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* windows,
    SessionID* active_window_id) {
  sessions::CommandStorageManagerTestHelper test_helper(
      service_->GetCommandStorageManagerForTest());
  std::vector<std::unique_ptr<sessions::SessionCommand>> read_commands =
      test_helper.ReadLastSessionCommands();
  RestoreSessionFromCommands(read_commands, windows, active_window_id);
  service_->RemoveUnusedRestoreWindows(windows);
}

void SessionServiceBaseTestHelper::AssertTabEquals(
    SessionID window_id,
    SessionID tab_id,
    int visual_index,
    int nav_index,
    size_t nav_count,
    const sessions::SessionTab& session_tab) {
  EXPECT_EQ(window_id.id(), session_tab.window_id.id());
  EXPECT_EQ(tab_id.id(), session_tab.tab_id.id());
  AssertTabEquals(visual_index, nav_index, nav_count, session_tab);
}

void SessionServiceBaseTestHelper::AssertTabEquals(
    int visual_index,
    int nav_index,
    size_t nav_count,
    const sessions::SessionTab& session_tab) {
  EXPECT_EQ(visual_index, session_tab.tab_visual_index);
  EXPECT_EQ(nav_index, session_tab.current_navigation_index);
  ASSERT_EQ(nav_count, session_tab.navigations.size());
}

// TODO(sky): nuke this and change to call directly into
// SerializedNavigationEntryTestHelper.
void SessionServiceBaseTestHelper::AssertNavigationEquals(
    const sessions::SerializedNavigationEntry& expected,
    const sessions::SerializedNavigationEntry& actual) {
  sessions::SerializedNavigationEntryTestHelper::ExpectNavigationEquals(
      expected, actual);
}

void SessionServiceBaseTestHelper::AssertSingleWindowWithSingleTab(
    const std::vector<std::unique_ptr<sessions::SessionWindow>>& windows,
    size_t nav_count) {
  ASSERT_EQ(1U, windows.size());
  EXPECT_EQ(1U, windows[0]->tabs.size());
  EXPECT_EQ(nav_count, windows[0]->tabs[0]->navigations.size());
}

void SessionServiceBaseTestHelper::SetServiceBase(SessionServiceBase* service) {
  service_ = service;
  // Execute IO tasks posted by the SessionService.
  content::RunAllTasksUntilIdle();
}

void SessionServiceBaseTestHelper::RunTaskOnBackendThread(
    const base::Location& from_here,
    base::OnceClosure task) {
  sessions::CommandStorageManagerTestHelper test_helper(
      service_->GetCommandStorageManagerForTest());
  test_helper.RunTaskOnBackendThread(from_here, std::move(task));
}

scoped_refptr<base::SequencedTaskRunner>
SessionServiceBaseTestHelper::GetBackendTaskRunner() {
  return sessions::CommandStorageManagerTestHelper(
             service_->GetCommandStorageManagerForTest())
      .GetBackendTaskRunner();
}

void SessionServiceBaseTestHelper::SetAvailableRange(
    SessionID tab_id,
    const std::pair<int, int>& range) {
  service_->SetAvailableRangeForTest(tab_id, range);
}

bool SessionServiceBaseTestHelper::GetAvailableRange(
    SessionID tab_id,
    std::pair<int, int>* range) {
  return service_->GetAvailableRangeForTest(tab_id, range);
}
