// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/token.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/base_session_service_delegate.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sessions {
class SessionCommand;
struct SessionTab;
struct SessionWindow;
}  // namespace sessions

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
// flushed to |SessionBackend| and written to a file. Every so often
// |SessionService| rebuilds the contents of the file from the open state of the
// browser.
class SessionService : public sessions::BaseSessionServiceDelegate,
                       public KeyedService,
                       public BrowserListObserver {
  friend class SessionServiceTestHelper;
 public:
  // Creates a SessionService for the specified profile.
  explicit SessionService(Profile* profile);
  // For testing.
  explicit SessionService(const base::FilePath& save_path);

  ~SessionService() override;

  // This may be NULL during testing.
  Profile* profile() const { return profile_; }

  // Returns true if a new window opening should really be treated like the
  // start of a session (with potential session restore, startup URLs, etc.).
  // In particular, this is true if there are no tabbed browsers running
  // currently (eg. because only background or other app pages are running).
  bool ShouldNewWindowStartSession();

  // Invoke at a point when you think session restore might occur. For example,
  // during startup and window creation this is invoked to see if a session
  // needs to be restored. If a session needs to be restored it is done so
  // asynchronously and true is returned. If false is returned the session was
  // not restored and the caller needs to create a new window.
  bool RestoreIfNecessary(const std::vector<GURL>& urls_to_open);

  // Resets the contents of the file from the current state of all open
  // browsers whose profile matches our profile.
  void ResetFromCurrentBrowsers();

  // Moves the current session to the last session. This is useful when a
  // checkpoint occurs, such as when the user launches the app and no tabbed
  // browsers are running.
  void MoveCurrentSessionToLastSession();

  // Deletes the last session.
  void DeleteLastSession();

  // Associates a tab with a window.
  void SetTabWindow(const SessionID& window_id,
                    const SessionID& tab_id);

  // Sets the bounds of a window.
  void SetWindowBounds(const SessionID& window_id,
                       const gfx::Rect& bounds,
                       ui::WindowShowState show_state);

  // Sets the workspace the window resides in.
  void SetWindowWorkspace(const SessionID& window_id,
                          const std::string& workspace);

  // Sets the visual index of the tab in its parent window.
  void SetTabIndexInWindow(const SessionID& window_id,
                           const SessionID& tab_id,
                           int new_index);

  // Sets a tab's group ID, if any. Note that a group can't be split between
  // multiple windows.
  void SetTabGroup(const SessionID& window_id,
                   const SessionID& tab_id,
                   base::Optional<base::Token> group);

  // Updates the metadata associated with a tab group. |window_id| should be the
  // window where the group currently resides. Note that a group can't be split
  // between multiple windows.
  void SetTabGroupMetadata(const SessionID& window_id,
                           const base::Token& group_id,
                           const base::string16& title,
                           SkColor color);

  // Sets the pinned state of the tab.
  void SetPinnedState(const SessionID& window_id,
                      const SessionID& tab_id,
                      bool is_pinned);

  // Notification that a tab has been closed. |closed_by_user_gesture| comes
  // from |WebContents::closed_by_user_gesture|; see it for details.
  //
  // Note: this is invoked from the NavigationController's destructor, which is
  // after the actual tab has been removed.
  void TabClosed(const SessionID& window_id,
                 const SessionID& tab_id,
                 bool closed_by_user_gesture);

  // Notification a window has opened.
  void WindowOpened(Browser* browser);

  // Notification the window is about to close.
  void WindowClosing(const SessionID& window_id);

  // Notification a window has finished closing.
  void WindowClosed(const SessionID& window_id);

  // Called when a tab is inserted.
  void TabInserted(content::WebContents* contents);

  // Called when a tab is closing.
  void TabClosing(content::WebContents* contents);

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (ShouldRestoreOfWindowType returns true).
  void SetWindowType(const SessionID& window_id, Browser::Type type);

  // Sets the application name of the specified window.
  void SetWindowAppName(const SessionID& window_id,
                        const std::string& app_name);

  // Invoked when the NavigationController has removed entries from the list.
  // |index| gives the the starting index from which entries were deleted.
  // |count| gives the number of entries that were removed.
  void TabNavigationPathPruned(const SessionID& window_id,
                               const SessionID& tab_id,
                               int index,
                               int count);

  // Invoked when the NavigationController has deleted entries because of a
  // history deletion.
  void TabNavigationPathEntriesDeleted(const SessionID& window_id,
                                       const SessionID& tab_id);

  // Updates the navigation entry for the specified tab.
  void UpdateTabNavigation(
      const SessionID& window_id,
      const SessionID& tab_id,
      const sessions::SerializedNavigationEntry& navigation);

  // Notification that a tab has restored its entries or a closed tab is being
  // reused.
  void TabRestored(content::WebContents* tab, bool pinned);

  // Sets the index of the selected entry in the navigation controller for the
  // specified tab.
  void SetSelectedNavigationIndex(const SessionID& window_id,
                                  const SessionID& tab_id,
                                  int index);

  // Sets the index of the selected tab in the specified window.
  void SetSelectedTabInWindow(const SessionID& window_id, int index);

  // Sets the user agent override of the specified tab.
  void SetTabUserAgentOverride(const SessionID& window_id,
                               const SessionID& tab_id,
                               const std::string& user_agent_override);

  // Sets the application extension id of the specified tab.
  void SetTabExtensionAppID(const SessionID& window_id,
                            const SessionID& tab_id,
                            const std::string& extension_app_id);

  // Sets the last active time of the tab.
  void SetLastActiveTime(const SessionID& window_id,
                         const SessionID& tab_id,
                         base::TimeTicks last_active_time);

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  base::CancelableTaskTracker::TaskId GetLastSession(
      const sessions::GetLastSessionCallback& callback,
      base::CancelableTaskTracker* tracker);

  // BaseSessionServiceDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;

 private:
  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, SavedSessionNotification);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RestoreActivation1);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RestoreActivation2);
  FRIEND_TEST_ALL_PREFIXES(SessionServiceTest, RemoveUnusedRestoreWindowsTest);
  FRIEND_TEST_ALL_PREFIXES(NoStartupWindowTest, DontInitSessionServiceForApps);

  typedef std::map<SessionID, std::pair<int, int>> IdToRange;

  void Init();

  // Returns true if a window of given |window_type| should get
  // restored upon session restore.
  bool ShouldRestoreWindowOfType(
      sessions::SessionWindow::WindowType type) const;

  // Removes unrestorable windows from the previous windows list.
  void RemoveUnusedRestoreWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list);

  // Implementation of RestoreIfNecessary. If |browser| is non-null and we need
  // to restore, the tabs are added to it, otherwise a new browser is created.
  bool RestoreIfNecessary(const std::vector<GURL>& urls_to_open,
                          Browser* browser);

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {}
  void OnBrowserSetLastActive(Browser* browser) override;

  // Converts |commands| to SessionWindows and notifies the callback.
  void OnGotSessionCommands(
      const sessions::GetLastSessionCallback& callback,
      std::vector<std::unique_ptr<sessions::SessionCommand>> commands);

  // Adds commands to commands that will recreate the state of the specified
  // tab. This adds at most kMaxNavigationCountToPersist navigations (in each
  // direction from the current navigation index).
  // A pair is added to tab_to_available_range indicating the range of
  // indices that were written.
  void BuildCommandsForTab(const SessionID& window_id,
                           content::WebContents* tab,
                           int index_in_window,
                           base::Optional<base::Token> group,
                           bool is_pinned,
                           IdToRange* tab_to_available_range);

  // Adds commands to create the specified browser, and invokes
  // BuildCommandsForTab for each of the tabs in the browser. This ignores
  // any tabs not in the profile we were created with.
  void BuildCommandsForBrowser(Browser* browser,
                               IdToRange* tab_to_available_range,
                               std::set<SessionID>* windows_to_track);

  // Iterates over all the known browsers invoking BuildCommandsForBrowser.
  // This only adds browsers that should be tracked (|ShouldRestoreWindowOfType|
  // returns true). All browsers that are tracked are added to windows_to_track
  // (as long as it is non-null).
  void BuildCommandsFromBrowsers(IdToRange* tab_to_available_range,
                                 std::set<SessionID>* windows_to_track);

  // Schedules a reset of the existing commands. A reset means the contents
  // of the file are recreated from the state of the browser.
  void ScheduleResetCommands();

  // Schedules the specified command.
  void ScheduleCommand(std::unique_ptr<sessions::SessionCommand> command);

  // Converts all pending tab/window closes to commands and schedules them.
  void CommitPendingCloses();

  // Returns true if there is only one window open with a single tab that shares
  // our profile.
  bool IsOnlyOneTabLeft() const;

  // Returns true if there are open trackable browser windows whose ids do
  // match |window_id| with our profile. A trackable window is a window from
  // which |ShouldRestoreWindowOfType| returns true. See
  // |ShouldRestoreWindowOfType| for details.
  bool HasOpenTrackableBrowsers(const SessionID& window_id) const;

  // Returns true if changes to tabs in the specified window should be tracked.
  bool ShouldTrackChangesToWindow(const SessionID& window_id) const;

  // Returns true if we track changes to the specified browser.
  bool ShouldTrackBrowser(Browser* browser) const;

  // Will rebuild session commands if rebuild_on_next_save_ is true.
  void RebuildCommandsIfRequired();

  // Call when certain session relevant notifications
  // (tab_closed, nav_list_pruned) occur.  In addition, this is
  // currently called when Save() is called to compare how often the
  // session data is currently saved verses when we may want to save it.
  // It records the data in UMA stats.
  void RecordSessionUpdateHistogramData(int type,
    base::TimeTicks* last_updated_time);

  // Deletes session data if no windows are open for the current profile.
  void MaybeDeleteSessionOnlyData();

  // Unit test accessors.
  sessions::BaseSessionService* GetBaseSessionServiceForTest();

  void SetAvailableRangeForTest(const SessionID& tab_id,
                                const std::pair<int, int>& range);
  bool GetAvailableRangeForTest(const SessionID& tab_id,
                                std::pair<int, int>* range);

  // The profile. This may be null during testing.
  Profile* profile_;

  // Whether to use delayed save. Set to false when constructed with a FilePath
  // (which should only be used for testing).
  bool should_use_delayed_save_;

  // The owned BaseSessionService.
  std::unique_ptr<sessions::BaseSessionService> base_session_service_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  //
  // This is only used if not all the navigation entries have been
  // written.
  IdToRange tab_to_available_range_;

  // When the user closes the last window, where the last window is the
  // last tabbed browser and no more tabbed browsers are open with the same
  // profile, the window ID is added here. These IDs are only committed (which
  // marks them as closed) if the user creates a new tabbed browser.
  typedef std::set<SessionID> PendingWindowCloseIDs;
  PendingWindowCloseIDs pending_window_close_ids_;

  // Set of tabs that have been closed by way of the last window or last tab
  // closing, but not yet committed.
  typedef std::set<SessionID> PendingTabCloseIDs;
  PendingTabCloseIDs pending_tab_close_ids_;

  // When a window other than the last window (see description of
  // pending_window_close_ids) is closed, the id is added to this set.
  typedef std::set<SessionID> WindowClosingIDs;
  WindowClosingIDs window_closing_ids_;

  // Set of windows we're tracking changes to. This is only browsers that
  // return true from |ShouldRestoreWindowOfType|.
  typedef std::set<SessionID> WindowsTracking;
  WindowsTracking windows_tracking_;

  // Are there any open trackable browsers?
  bool has_open_trackable_browsers_;

  // If true and a new tabbed browser is created and there are no opened tabbed
  // browser (has_open_trackable_browsers_ is false), then the current session
  // is made the last session. See description above class for details on
  // current/last session.
  bool move_on_new_browser_;

  // For browser_tests, since we want to simulate the browser shutting down
  // without quitting.
  bool force_browser_not_alive_with_no_windows_;

  // Force session commands to be rebuild before next save event.
  bool rebuild_on_next_save_;

  // Don't send duplicate SetSelectedTabInWindow commands when the selected
  // tab's index hasn't changed.
  std::map<SessionID, int> last_selected_tab_in_window_;

  base::WeakPtrFactory<SessionService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionService);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_H_
