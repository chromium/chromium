// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_

#include <map>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/ui/browser.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sessions {
struct SessionWindow;
}  // namespace sessions

struct StartupTab;
using StartupTabs = std::vector<StartupTab>;

// SessionService ------------------------------------------------------------

// SessionService is responsible for maintaining the state of open windows
// and tabs so that they can be restored at a later date. The state of the
// currently open browsers is referred to as the current session.
//
// SessionService supports restoring from the last session. The last session
// typically corresponds to the last run of the browser, but not always. For
// example, if the user has a tabbed browser and app window running, closes the
// tabbed browser, then creates a new tabbed browser the current session is made
// the last session and the current session reset. This is done to provide the
// illusion that app windows run in separate processes. Similar behavior occurs
// with incognito windows.
//
// SessionService itself uses functions from session_service_commands to store
// commands which can rebuild the open state of the browser (as |SessionWindow|,
// |SessionTab| and |SerializedNavigationEntry|). The commands are periodically
// flushed to |CommandStorageBackend| and written to a file. Every so often
// |SessionService| rebuilds the contents of the file from the open state of the
// browser.

// TODO(stahon@microsoft.com) When AppSessionService is implemented, we should
// make a pass in SessionService to remove app related code.
class SessionService : public SessionServiceBase {
  friend class SessionServiceTestHelper;
 public:
  // Creates a SessionService for the specified profile.
  explicit SessionService(Profile* profile);

  SessionService(const SessionService&) = delete;
  SessionService& operator=(const SessionService&) = delete;

  ~SessionService() override;

  // Returns true if `window_type` identifies a type tracked by SessionService.
  static bool IsRelevantWindowType(
      sessions::SessionWindow::WindowType window_type);

  // Returns true if restore should be triggered. If `browser` is non-null this
  // is called as the result of a new Browser being created. If `browser` is
  // null this is called from RestoreIfNecessary();
  bool ShouldRestore(Browser* browser);

  // Invoke at a point when you think session restore might occur. For example,
  // during startup and window creation this is invoked to see if a session
  // needs to be restored. If a session needs to be restored it is done so
  // asynchronously and true is returned. If false is returned the session was
  // not restored and the caller needs to create a new window.
  // Since RestoreIfNecessary can potentially trigger a restore, we need to
  // know whether the caller intends for us to restore apps or not.
  bool RestoreIfNecessary(const StartupTabs& startup_tabs, bool restore_apps);

  // Moves the current session to the last session. This is useful when a
  // checkpoint occurs, such as when the user launches the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

  // Deletes the last session.
  void DeleteLastSession();

  // Sets a tab's group ID, if any. Note that a group can't be split between
  // multiple windows.
  void SetTabGroup(SessionID window_id,
                   SessionID tab_id,
                   std::optional<tab_groups::TabGroupId> group);

  // Updates the metadata associated with a tab group. |window_id| should be
  // the window where the group currently resides. Note that a group can't be
  // split between multiple windows.
  void SetTabGroupMetadata(
      SessionID window_id,
      const tab_groups::TabGroupId& group_id,
      const tab_groups::TabGroupVisualData* visual_data,
      std::optional<std::string> saved_guid = std::nullopt);

  void AddTabExtraData(SessionID window_id,
                       SessionID tab_id,
                       const char* key,
                       const std::string& data);

  void AddWindowExtraData(SessionID window_id,
                          const char* key,
                          const std::string& data);

  void TabClosed(SessionID window_id, SessionID tab_id) override;

  // Notification a window has opened.
  void WindowOpened(Browser* browser) override;

  // Notification the window is about to close.
  void WindowClosing(SessionID window_id) override;

  // Notification a window has finished closing.
  void WindowClosed(SessionID window_id) override;

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (ShouldRestoreOfWindowType returns true).
  void SetWindowType(SessionID window_id, Browser::Type type) override;

  void SetWindowUserTitle(SessionID window_id, const std::string& user_title);

  // CommandStorageManagerDelegate:
  void OnErrorWritingSessionCommands() override;

  void SetTabUserAgentOverride(SessionID window_id,
                               SessionID tab_id,
                               const sessions::SerializedUserAgentOverride&
                                   user_agent_override) override;

  int count_delete_last_session_for_testing() const {
    return count_delete_last_session_for_testing_;
  }

 protected:
  Browser::Type GetDesiredBrowserTypeForWebContents() override;
  void DidScheduleCommand() override;

 private:
  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, SavedSessionNotification);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RestoreActivation1);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RestoreActivation2);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RemoveUnusedRestoreWindowsTest);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, Workspace);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, WorkspaceSavedOnOpened);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, VisibleOnAllWorkspaces);
  FRIEND_TEST_ALL_PREFIXES(NoStartupWindowTest, DontInitSessionServiceForApps);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, PinnedAfterReset);

  using IdToRange = std::map<SessionID, std::pair<int, int>>;

  // Returns true if a window of given |window_type| should get
  // restored upon session restore.
  bool ShouldRestoreWindowOfType(
      sessions::SessionWindow::WindowType type) const override;

  // Implementation of RestoreIfNecessary. If |browser| is non-null and we
  // need to restore, the tabs are added to it, otherwise a new browser is
  // created.
  bool RestoreIfNecessary(const StartupTabs& startup_tabs,
                          Browser* browser,
                          bool restore_apps);

  // Adds commands to commands that will recreate the state of the specified
  // tab. This adds at most kMaxNavigationCountToPersist navigations (in each
  // direction from the current navigation index).
  // A pair is added to tab_to_available_range indicating the range of
  // indices that were written.
  void BuildCommandsForTab(SessionID window_id,
                           content::WebContents* tab,
                           int index_in_window,
                           std::optional<tab_groups::TabGroupId> group,
                           bool is_pinned,
                           IdToRange* tab_to_available_range) override;

  // Schedules a reset of the existing commands. A reset means the contents
  // of the file are recreated from the state of the browser.
  void ScheduleResetCommands() override;

  // Converts all pending tab/window closes to commands and schedules them.
  void CommitPendingCloses();

  // Returns true if there is only one window open with a single tab that
  // shares our profile.
  bool IsOnlyOneTabLeft() const;

  // Returns true if there are open trackable browser windows whose ids do
  // match |window_id| with our profile. A trackable window is a window from
  // which |ShouldRestoreWindowOfType| returns true. See
  // |ShouldRestoreWindowOfType| for details.
  bool HasOpenTrackableBrowsers(SessionID window_id) const;

  // Will rebuild session commands if rebuild_on_next_save_ is true.
  void RebuildCommandsIfRequired() override;

  // Invoked with true when all browsers start closing.
  void OnClosingAllBrowsersChanged(bool closing);

  // If necessary, removes the current exit event and adds a new one. This
  // does nothing if `pending_window_close_ids_` is empty, which means the
  // user is potentially closing the last browser.
  void LogExitEvent();

  // If an exit event was logged, it is removed.
  void RemoveExitEvent();

  // When the user closes the last window, where the last window is the
  // last tabbed browser and no more tabbed browsers are open with the same
  // profile, the window ID is added here. These IDs are only committed (which
  // marks them as closed) if the user creates a new tabbed browser.
  using PendingWindowCloseIDs = std::set<SessionID>;
  PendingWindowCloseIDs pending_window_close_ids_;

  // Set of tabs that have been closed by way of the last window or last tab
  // closing, but not yet committed.
  using PendingTabCloseIDs = std::set<SessionID>;
  PendingTabCloseIDs pending_tab_close_ids_;

  // When a window other than the last window (see description of
  // pending_window_close_ids) is closed, the id is added to this set.
  using WindowClosingIDs = std::set<SessionID>;
  WindowClosingIDs window_closing_ids_;

  // Are there any open trackable browsers?
  bool has_open_trackable_browsers_ = false;

  // Used to override HasOpenTrackableBrowsers()
  bool has_open_trackable_browser_for_test_ = true;

  // Use to override IsOnlyOneTableft()
  bool is_only_one_tab_left_for_test_ = false;

  // The number of times `DeleteLastSession()` has been invoked for the current
  // session service instance.
  int count_delete_last_session_for_testing_ = 0;

  // If true and a new tabbed browser is created and there are no opened
  // tabbed browser (has_open_trackable_browsers_ is false), then the current
  // session is made the last session. See description above class for details
  // on current/last session.
  bool move_on_new_browser_ = false;

  // For browser_tests, since we want to simulate the browser shutting down
  // without quitting.
  bool force_browser_not_alive_with_no_windows_ = false;

  base::CallbackListSubscription closing_all_browsers_subscription_;

  bool did_log_exit_ = false;

  int unrecoverable_write_error_count_ = 0;

  // True if this is the first SessionService created for the Profile. A value
  // of false means the first SessionService was destroyed and a new one
  // created.
  const bool is_first_session_service_;

  // Set to true once a valid command has been scheduled.
  bool did_schedule_command_ = false;

  base::WeakPtrFactory<SessionService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
