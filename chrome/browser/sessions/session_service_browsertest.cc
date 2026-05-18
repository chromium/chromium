// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include "base/containers/adapters.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#endif

class SessionServiceBrowserTest : public InProcessBrowserTest {
 protected:
  SessionService* service() {
    return SessionServiceFactory::GetForProfile(browser()->profile());
  }

  std::optional<SessionServiceEvent> FindMostRecentEventOfType(
      SessionServiceEventLogType type) {
    auto events = GetSessionServiceEvents(browser()->profile());
    for (const SessionServiceEvent& event : base::Reversed(events)) {
      if (event.type == type) {
        return event;
      }
    }
    return std::nullopt;
  }
};

// Tests that the workspace is saved in the browser session during
// SetWindowWorkspace.
IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, Workspace) {
  service()->ResetFromCurrentBrowsers();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_workspace_command = false;
  std::string expected_workspace = browser()->window()->GetWorkspace();
  std::unique_ptr<sessions::SessionCommand> workspace_command =
      sessions::CreateSetWindowWorkspaceCommand(browser()->session_id(),
                                                expected_workspace);
  for (const auto& command : pending_commands) {
    if (command->id() == workspace_command->id() &&
        command->contents() == workspace_command->contents()) {
      found_workspace_command = true;
      break;
    }
  }
  EXPECT_TRUE(found_workspace_command);
}

// Tests that the workspace is saved in the browser session during
// `SessionService::WindowOpened(),` called in `Browser()` constructor to
// save the current workspace to newly created browser.
IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, WorkspaceSavedOnOpened) {
  // Clear any pending commands first.
  SessionServiceTestHelper helper(service());
  helper.command_storage_manager()->ClearPendingCommands();

  // Call WindowOpened manually on the browser, exactly like the original unit test.
  service()->WindowOpened(browser());

  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = helper.command_storage_manager()->pending_commands();
  bool found_workspace_command = false;
  std::string expected_workspace = browser()->window()->GetWorkspace();
  std::unique_ptr<sessions::SessionCommand> workspace_command =
      sessions::CreateSetWindowWorkspaceCommand(browser()->session_id(),
                                                expected_workspace);
  for (const auto& command : pending_commands) {
    if (command->id() == workspace_command->id() &&
        command->contents() == workspace_command->contents()) {
      found_workspace_command = true;
      break;
    }
  }
  EXPECT_TRUE(found_workspace_command);
}

// Tests that the visible on all workspaces state is saved during
// SessionService::BuildCommandsForBrowser.
IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, VisibleOnAllWorkspaces) {
  service()->ResetFromCurrentBrowsers();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_visible_on_all_workspaces_command = false;
  bool expected_visible = browser()->window()->IsVisibleOnAllWorkspaces();
  std::unique_ptr<sessions::SessionCommand> visible_on_all_workspaces_command =
      sessions::CreateSetWindowVisibleOnAllWorkspacesCommand(
          browser()->session_id(), expected_visible);
  for (const auto& command : pending_commands) {
    if (command->id() == visible_on_all_workspaces_command->id() &&
        command->contents() == visible_on_all_workspaces_command->contents()) {
      found_visible_on_all_workspaces_command = true;
      break;
    }
  }
  EXPECT_TRUE(found_visible_on_all_workspaces_command);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, PinnedAfterReset) {
  browser()->tab_strip_model()->SetTabPinned(0, true);
  // Force a reset, to verify that SessionService::BuildCommandsForBrowser
  // handles pinned tabs correctly.
  service()->ResetFromCurrentBrowsers();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_pinned_command = false;

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          browser()->tab_strip_model()->GetWebContentsAt(0));
  std::unique_ptr<sessions::SessionCommand> pinned_command =
      sessions::CreatePinnedStateCommand(session_tab_helper->session_id(),
                                         true);

  for (const auto& command : pending_commands) {
    if (command->id() == pinned_command->id() &&
        command->contents() == pinned_command->contents()) {
      found_pinned_command = true;
      break;
    }
  }
  EXPECT_TRUE(found_pinned_command);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, LogExit) {
  EXPECT_FALSE(FindMostRecentEventOfType(SessionServiceEventLogType::kExit));
  service()->WindowClosing(browser()->session_id());
  auto exit_event =
      FindMostRecentEventOfType(SessionServiceEventLogType::kExit);
  ASSERT_TRUE(exit_event);
  EXPECT_EQ(1, exit_event->data.exit.window_count);
  EXPECT_EQ(browser()->tab_strip_model()->count(),
            exit_event->data.exit.tab_count);

  // Create another window, which should remove the exit.
  SessionID window2_id = SessionID::NewUnique();
  service()->SetWindowType(window2_id, Browser::TYPE_NORMAL);
  EXPECT_FALSE(FindMostRecentEventOfType(SessionServiceEventLogType::kExit));
}
