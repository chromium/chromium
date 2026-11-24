// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <ranges>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#endif

class SessionServiceBrowserTest : public InProcessBrowserTest {
 protected:
  SessionService* service() {
    return SessionServiceFactory::GetForProfile(browser()->GetProfile());
  }

  std::optional<SessionServiceEvent> FindMostRecentEventOfType(
      SessionServiceEventLogType type) {
    auto events = GetSessionServiceEvents(browser()->GetProfile());
    for (const SessionServiceEvent& event : std::views::reverse(events)) {
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
  service()->ResetFromCurrentBrowsersForTesting();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_workspace_command = false;
  std::string expected_workspace =
      BrowserWindow::FromBrowser(browser())->GetWorkspace();
  std::unique_ptr<sessions::SessionCommand> workspace_command =
      sessions::CreateSetWindowWorkspaceCommand(browser()->GetSessionID(),
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
  std::string expected_workspace =
      BrowserWindow::FromBrowser(browser())->GetWorkspace();
  std::unique_ptr<sessions::SessionCommand> workspace_command =
      sessions::CreateSetWindowWorkspaceCommand(browser()->GetSessionID(),
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
  service()->ResetFromCurrentBrowsersForTesting();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_visible_on_all_workspaces_command = false;
  bool expected_visible =
      BrowserWindow::FromBrowser(browser())->IsVisibleOnAllWorkspaces();
  std::unique_ptr<sessions::SessionCommand> visible_on_all_workspaces_command =
      sessions::CreateSetWindowVisibleOnAllWorkspacesCommand(
          browser()->GetSessionID(), expected_visible);
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
  browser()->GetTabStripModel()->SetTabPinned(0, true);
  // Force a reset, to verify that SessionService::BuildCommandsForBrowser
  // handles pinned tabs correctly.
  service()->ResetFromCurrentBrowsersForTesting();

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();
  bool found_pinned_command = false;

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          browser()->GetTabStripModel()->GetWebContentsAt(0));
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
  service()->WindowClosing(browser()->GetSessionID());
  auto exit_event =
      FindMostRecentEventOfType(SessionServiceEventLogType::kExit);
  ASSERT_TRUE(exit_event);
  EXPECT_EQ(1, exit_event->data.exit.window_count);
  EXPECT_EQ(browser()->GetTabStripModel()->count(),
            exit_event->data.exit.tab_count);

  // Create another window, which should remove the exit.
  SessionID window2_id = SessionID::NewUnique();
  service()->SetWindowType(window2_id,
                           BrowserWindowInterface::Type::TYPE_NORMAL);
  EXPECT_FALSE(FindMostRecentEventOfType(SessionServiceEventLogType::kExit));
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest,
                       ResetDuringLastWindowTeardownKeepsAllTabs) {
  base::HistogramTester histogram_tester;
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      browser()->GetProfile(), ProfileKeepAliveOrigin::kSessionRestore);

  SessionServiceTestHelper helper(service());
  service()->ResetFromCurrentBrowsersForTesting();

  AddBlankTabAndShow(browser());
  AddBlankTabAndShow(browser());
  SessionID window_id = browser()->GetSessionID();

  helper.command_storage_manager()->Save();

  // Crossing the rewrite threshold with the window open loses nothing.
  for (int i = helper.command_storage_manager()->commands_since_reset();
       i < SessionServiceBase::kWritesPerReset; ++i) {
    service()->SetWindowVisibleOnAllWorkspaces(window_id, true);
  }
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.RebuildWouldHaveDroppedTabs", false, 1);
  // Performs and writes the deferred rewrite, restarting the count.
  helper.command_storage_manager()->Save();

  int current = helper.command_storage_manager()->commands_since_reset();
  int target = SessionServiceBase::kWritesPerReset - 1;
  for (int i = current; i < target; ++i) {
    service()->SetWindowVisibleOnAllWorkspaces(window_id, true);
  }

  Profile* profile = browser()->GetProfile();

  // We manually call WindowClosing() because we must push past kWritesPerReset
  // precisely in the middle of teardown, which a real close path won't let us
  // intercept easily.
  service()->WindowClosing(window_id);
  browser()->GetTabStripModel()->CloseWebContentsAt(
      2, TabCloseTypes::CLOSE_USER_GESTURE);

  // Push past kWritesPerReset DURING teardown.
  service()->SetWindowVisibleOnAllWorkspaces(window_id, true);
  histogram_tester.ExpectBucketCount(
      "Session.Counterfactual.RebuildWouldHaveDroppedTabs", true, 1);

  // Finish closing the browser.
  CloseBrowserSynchronously(browser());

  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(profile, pref);
  BrowserWindowInterface* new_browser = CreateBrowser(profile);

  base::test::TestFuture<std::vector<std::unique_ptr<sessions::SessionWindow>>,
                         SessionID, sessions::CommandStorageReadStatus,
                         sessions::SessionReplayResult>
      future;
  SessionServiceFactory::GetForProfile(new_browser->GetProfile())
      ->GetLastSession(future.GetCallback());

  auto windows = std::get<0>(future.Take());

  bool found = false;
  for (const auto& window : windows) {
    if (window->window_id == window_id) {
      EXPECT_EQ(3u, window->tabs.size());
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, EmptyWindowSaved) {
  base::HistogramTester histogram_tester;
  BrowserWindowInterface* empty_browser = CreateBrowserWindowForTesting(
      BrowserWindowCreateParams(browser()->GetProfile(), true));

  ASSERT_EQ(0, empty_browser->GetTabStripModel()->count());
  service()->ResetFromCurrentBrowsersForTesting();

  // A window that already had tabs at the rebuild is not counted.
  std::ignore = chrome::AddSelectedTabWithURL(
      browser(), GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  histogram_tester.ExpectTotalCount(
      "Session.Counterfactual.EmptyWindowTabWouldHaveBeenLost", 0);

  std::ignore = chrome::AddSelectedTabWithURL(
      empty_browser, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED);
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.EmptyWindowTabWouldHaveBeenLost", true, 1);

  sessions::CommandStorageManager* command_storage_manager =
      SessionServiceTestHelper(service()).command_storage_manager();
  const std::vector<std::unique_ptr<sessions::SessionCommand>>&
      pending_commands = command_storage_manager->pending_commands();

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          empty_browser->GetTabStripModel()->GetWebContentsAt(0));
  std::unique_ptr<sessions::SessionCommand> set_tab_window_command =
      sessions::CreateSetTabWindowCommand(empty_browser->GetSessionID(),
                                          session_tab_helper->session_id());

  bool found = false;
  for (const auto& command : pending_commands) {
    if (command->id() == set_tab_window_command->id() &&
        command->contents() == set_tab_window_command->contents()) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest, NoRewriteOnSecondNavigation) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,first")));

  service()->ResetFromCurrentBrowsersForTesting();
  SessionServiceTestHelper helper(service());
  EXPECT_TRUE(helper.command_storage_manager()->pending_reset());
  helper.command_storage_manager()->Save();
  EXPECT_FALSE(helper.command_storage_manager()->pending_reset());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::pair<int, int> range;
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab);
  ASSERT_TRUE(
      helper.GetAvailableRange(session_tab_helper->session_id(), &range));
  EXPECT_EQ(tab->GetController().GetLastCommittedEntryIndex(), range.second);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,second")));
  EXPECT_FALSE(helper.command_storage_manager()->pending_reset());
  // The old code would not have rewritten here either.
  histogram_tester.ExpectTotalCount("Session.Counterfactual.FullRewriteAvoided",
                                    0);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,third")));
  EXPECT_FALSE(helper.command_storage_manager()->pending_reset());
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.FullRewriteAvoided", true, 1);

  // The old code's rewrite reset the bound, so it skipped the 4th page and
  // rewrote again on the 5th.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,fourth")));
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.FullRewriteAvoided", true, 1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,fifth")));
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.FullRewriteAvoided", true, 2);

  // No rewrite is avoided when one is already pending.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,sixth")));
  helper.command_storage_manager()->Save();
  helper.command_storage_manager()->set_pending_reset(true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,seventh")));
  histogram_tester.ExpectUniqueSample(
      "Session.Counterfactual.FullRewriteAvoided", true, 2);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest,
                       CommittedPendingCloseCountedAsOpenMetric) {
  constexpr char kHistogram[] =
      "Session.WindowClosing.CommittedPendingCloseCountedAsOpen";
  base::HistogramTester histogram_tester;
  Profile* profile = browser()->GetProfile();

  // Closing a normal non-last window records a single `false` baseline sample
  // even though `BrowserView::OnWindowCloseRequested()` invokes
  // `WindowClosing()` twice (before and after `CloseAllTabs()`).
  BrowserWindowInterface* extra_browser = CreateBrowser(profile);
  CloseBrowserSynchronously(extra_browser);
  histogram_tester.ExpectUniqueSample(kHistogram, false, 1);

  // Window 1 starts closing as the last window (`pending_window_close_ids_`),
  // recording a `false` baseline sample, but has not finished asynchronous
  // teardown (`WindowClosed()`) yet.
  service()->WindowClosing(browser()->GetSessionID());
  histogram_tester.ExpectUniqueSample(kHistogram, false, 2);

  // Window 2 opens while Window 1 is still alive, committing Window 1's
  // pending close (`CommitPendingCloses()`).
  BrowserWindowInterface* browser2 = CreateBrowser(profile);
  histogram_tester.ExpectUniqueSample(kHistogram, false, 2);

  // Window 2 now closes before Window 1's `WindowClosed()` runs. Because
  // Window 1 is still in the browser collection and not in
  // `window_closing_ids_`, `HasOpenTrackableBrowsers()` counts Window 1 as
  // open. Even though `BrowserView::OnWindowCloseRequested()` invokes
  // `WindowClosing()` twice, only a single `true` sample should be recorded.
  CloseBrowserSynchronously(browser2);
  histogram_tester.ExpectBucketCount(kHistogram, false, 2);
  histogram_tester.ExpectBucketCount(kHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kHistogram, 3);

  // Closing an untracked window (e.g. `omit_from_session_restore` or
  // `TYPE_DEVTOOLS`) while Window 1 is still in
  // `committed_pending_window_close_ids_` invokes `WindowClosed()` ->
  // `HasOpenTrackableBrowsers()`, which must not record an extra sample.
  BrowserWindowCreateParams untracked_params(profile, true);
  untracked_params.omit_from_session_restore = true;
  BrowserWindowInterface* untracked_browser =
      CreateBrowserWindowForTesting(std::move(untracked_params));
  CloseBrowserSynchronously(untracked_browser);
  histogram_tester.ExpectTotalCount(kHistogram, 3);

  // Finishing teardown of Window 1 (already in
  // `committed_pending_window_close_ids_`) must not record another sample.
  CloseBrowserSynchronously(browser());
  histogram_tester.ExpectBucketCount(kHistogram, false, 2);
  histogram_tester.ExpectBucketCount(kHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kHistogram, 3);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest,
                       ResetFromCurrentBrowsersDuringTeardownMetrics) {
  base::HistogramTester histogram_tester;

  // AppSessionService resets must not emit into the SessionService histogram.
  AppSessionService* app_service =
      AppSessionServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(app_service);
  app_service->ResetFromCurrentBrowsers(
      SessionServiceBase::ResetReason::kWebAppReparenting);
  histogram_tester.ExpectTotalCount(
      "Session.ResetFromCurrentBrowsers.ServiceStartup.HasPendingTabCloses", 0);
  histogram_tester.ExpectTotalCount(
      "Session.ResetFromCurrentBrowsers.WebAppReparenting.HasPendingTabCloses",
      0);

  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(active_tab);

  // Immediate resets while no window is closing record false for their caller
  // reason.
  service()->ResetFromCurrentBrowsers(
      SessionServiceBase::ResetReason::kServiceStartup);
  histogram_tester.ExpectUniqueSample(
      "Session.ResetFromCurrentBrowsers.ServiceStartup.HasPendingTabCloses",
      false, 1);

  service()->ResetFromCurrentBrowsers(
      SessionServiceBase::ResetReason::kWebAppReparenting);
  histogram_tester.ExpectUniqueSample(
      "Session.ResetFromCurrentBrowsers.WebAppReparenting.HasPendingTabCloses",
      false, 1);

  // Set an available range [2, 4] and trigger an out-of-range navigation index
  // while no window is closing.
  SessionServiceTestHelper helper(service());
  helper.SetAvailableRange(session_tab_helper->session_id(), {2, 4});
  service()->SetSelectedNavigationIndex(browser()->GetSessionID(),
                                        session_tab_helper->session_id(), 0);
  histogram_tester.ExpectUniqueSample(
      "Session.ResetFromCurrentBrowsers.NavigationIndexOutOfRange."
      "HasPendingTabCloses",
      false, 1);

  // Start closing the last window and close one of its tabs so
  // `pending_tab_close_ids_` is non-empty.
  const SessionID window_id = browser()->GetSessionID();
  service()->WindowClosing(window_id);
  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  // Trigger an out-of-range navigation index mid-teardown.
  content::WebContents* remaining_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  sessions::SessionTabHelper* remaining_tab_helper =
      sessions::SessionTabHelper::FromWebContents(remaining_tab);
  helper.SetAvailableRange(remaining_tab_helper->session_id(), {2, 4});
  service()->SetSelectedNavigationIndex(window_id,
                                        remaining_tab_helper->session_id(), 0);

  histogram_tester.ExpectBucketCount(
      "Session.ResetFromCurrentBrowsers.NavigationIndexOutOfRange."
      "HasPendingTabCloses",
      true, 1);

  // Trigger a web app reparenting reset mid-teardown as well.
  service()->ResetFromCurrentBrowsers(
      SessionServiceBase::ResetReason::kWebAppReparenting);
  histogram_tester.ExpectBucketCount(
      "Session.ResetFromCurrentBrowsers.WebAppReparenting.HasPendingTabCloses",
      true, 1);

  CloseBrowserSynchronously(browser());
}

namespace {

// Returns the total size of the session files (not the tab or app ones) in
// `dir`. Queries each file with GetFileSize() rather than
// FileEnumerator::FileInfo::GetSize() because on Windows, FindFirstFile()
// reads the directory entry, whose size stays 0 while the session file handle
// is still open for writing.
int64_t GetSessionFilesSize(const base::FilePath& dir) {
  int64_t size = 0;
  base::FileEnumerator files(
      dir, /*recursive=*/false, base::FileEnumerator::FILES,
      base::FilePath::StringType(sessions::kSessionFileNamePrefix) +
          FILE_PATH_LITERAL("_*"));
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    size += base::GetFileSize(path).value_or(0);
  }
  return size;
}

}  // namespace

// BrowserProcessImpl::EndSession() is called before the process is terminated
// at logout. Session commands are normally written on a timer that won't fire
// before then, so EndSession() must write them and wait for the write.
IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest,
                       EndSessionWritesPendingCommands) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SessionServiceTestHelper helper(service());
  sessions::CommandStorageManagerTestHelper storage_helper(
      helper.command_storage_manager());

  // Write whatever is pending, so that the size below only grows with the
  // commands added afterwards.
  helper.command_storage_manager()->Save();
  storage_helper.RunMessageLoopUntilBackendDone();
  const base::FilePath sessions_dir = browser()->GetProfile()->GetPath().Append(
      storage_helper.ShouldWriteCleartextFiles()
          ? sessions::kSessionsDirectory
          : sessions::kEncryptedSessionsDirectory);
  const int64_t size_before = GetSessionFilesSize(sessions_dir);
  ASSERT_GT(size_before, 0);

  service()->SetWindowBounds(browser()->GetSessionID(),
                             gfx::Rect(10, 20, 300, 400),
                             ui::mojom::WindowShowState::kNormal);
  ASSERT_TRUE(helper.HasPendingSave());

  g_browser_process->EndSession();

  // Read the files right away, without running any tasks: EndSession() must
  // have waited for the backend to write the commands.
  EXPECT_FALSE(helper.HasPendingSave());
  EXPECT_TRUE(helper.command_storage_manager()->pending_commands().empty());
  EXPECT_GT(GetSessionFilesSize(sessions_dir), size_before);
}

IN_PROC_BROWSER_TEST_F(SessionServiceBrowserTest,
                       UntrackedTrackableWindowChangeDropped) {
  constexpr char kHistogram[] =
      "Session.WindowTracking.UntrackedTrackableWindowChangeDropped";
  base::HistogramTester histogram_tester;
  SessionServiceTestHelper helper(service());

  // Reset per-rebuild tracking sets so startup events do not suppress the
  // baseline sample below.
  service()->ResetFromCurrentBrowsersForTesting();

  // Normal navigation in a tracked window records a single `false` baseline
  // sample per rebuild; subsequent tracked changes are deduplicated.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,first")));
  histogram_tester.ExpectUniqueSample(kHistogram, false, 1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,first_b")));
  histogram_tester.ExpectUniqueSample(kHistogram, false, 1);

  // Non-trackable open windows (e.g. `omit_from_session_restore = true`) must
  // not emit the metric on tab insertion, navigation, or tab selection.
  BrowserWindowCreateParams omit_params(browser()->GetProfile(), true);
  omit_params.omit_from_session_restore = true;
  BrowserWindowInterface* omit_browser =
      CreateBrowserWindowForTesting(std::move(omit_params));
  std::ignore =
      AddTabAtIndexToBrowser(omit_browser, -1, GURL("data:text/html,omit1"),
                             ui::PAGE_TRANSITION_TYPED);
  std::ignore =
      AddTabAtIndexToBrowser(omit_browser, -1, GURL("data:text/html,omit2"),
                             ui::PAGE_TRANSITION_TYPED);
  omit_browser->GetTabStripModel()->ActivateTabAt(0);
  histogram_tester.ExpectUniqueSample(kHistogram, false, 1);
  CloseBrowserSynchronously(omit_browser);

  // Simulate the window missing from `windows_tracking_` while remaining open
  // in `ProfileBrowserCollection` without triggering `WindowClosed()` side
  // effects.
  const SessionID window_id = browser()->GetSessionID();
  helper.EraseWindowFromTracking(window_id);

  // Non-restorable URLs passed to `UpdateTabNavigation()` must not emit the
  // metric even when the window is untracked.
  sessions::SessionTabHelper* tab_helper =
      sessions::SessionTabHelper::FromWebContents(
          browser()->GetTabStripModel()->GetActiveWebContents());
  sessions::SerializedNavigationEntry invalid_nav;
  invalid_nav.set_index(0);
  invalid_nav.set_virtual_url(GURL());
  service()->UpdateTabNavigation(window_id, tab_helper->session_id(),
                                 invalid_nav);
  sessions::SerializedNavigationEntry quit_nav;
  quit_nav.set_index(0);
  quit_nav.set_virtual_url(GURL(chrome::kChromeUIQuitURL));
  service()->UpdateTabNavigation(window_id, tab_helper->session_id(), quit_nav);
  histogram_tester.ExpectUniqueSample(kHistogram, false, 1);

  // `UpdateTabNavigation()` with a restorable URL on an untracked trackable
  // window emits a `true` sample.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,second")));
  histogram_tester.ExpectBucketCount(kHistogram, false, 1);
  histogram_tester.ExpectBucketCount(kHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kHistogram, 2);

  // Subsequent changes on the same untracked window before a rebuild are
  // deduplicated.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,third")));
  histogram_tester.ExpectTotalCount(kHistogram, 2);

  // Rebuilding via `ResetFromCurrentBrowsersForTesting()` clears
  // `tracked_windows_recorded_` and `untracked_windows_recorded_` and
  // re-tracks open trackable windows, so a tracked change records a new
  // `false` baseline sample.
  service()->ResetFromCurrentBrowsersForTesting();
  std::ignore = AddTabAtIndexToBrowser(
      browser(), -1, GURL("data:text/html,tab2"), ui::PAGE_TRANSITION_TYPED);
  histogram_tester.ExpectBucketCount(kHistogram, false, 2);
  histogram_tester.ExpectBucketCount(kHistogram, true, 1);
  histogram_tester.ExpectTotalCount(kHistogram, 3);

  // `TabInserted()` on an untracked trackable window after rebuild logs `true`
  // again.
  helper.EraseWindowFromTracking(window_id);
  service()->TabInserted(browser()->GetTabStripModel()->GetActiveWebContents());
  histogram_tester.ExpectBucketCount(kHistogram, false, 2);
  histogram_tester.ExpectBucketCount(kHistogram, true, 2);
  histogram_tester.ExpectTotalCount(kHistogram, 4);

  // Rebuild again and verify `SetSelectedTabInWindow()` on an untracked
  // trackable window also logs `true`.
  service()->ResetFromCurrentBrowsersForTesting();
  helper.EraseWindowFromTracking(window_id);
  browser()->GetTabStripModel()->ActivateTabAt(0);
  histogram_tester.ExpectBucketCount(kHistogram, false, 2);
  histogram_tester.ExpectBucketCount(kHistogram, true, 3);
  histogram_tester.ExpectTotalCount(kHistogram, 5);
}
