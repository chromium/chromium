// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/content/session_tab_helper_delegate.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/session_service_commands.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "ui/base/ui_base_types.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sessions {
class CommandStorageManager;
class SessionCommand;
struct SessionWindow;
}  // namespace sessions

// SessionServiceBase -----
// SessionServiceBase is used by SessionService and AppSessionService to
// restore browser windows and apps respectively. Base is never intended
// to be instantiated and contains the common functionality that both
// SessionService and AppSessionService share in common. Some functions
// are implemented by both if they have differing behavior while others
// exist in one but not the other.
class SessionServiceBase : public sessions::CommandStorageManagerDelegate,
                           public sessions::SessionTabHelperDelegate,
                           public KeyedService,
                           public BrowserListObserver {
  friend class SessionServiceTestHelper;

 public:
  enum class SessionServiceType { kAppRestore, kSessionRestore };

  ~SessionServiceBase() override;

  Profile* profile() const { return profile_; }

  // Sets whether the window is visible on all workspaces or not.
  void SetWindowVisibleOnAllWorkspaces(const SessionID& window_id,
                                       bool visible_on_all_workspaces);

  // Resets the contents of the file from the current state of all open
  // apps whose profile matches our profile.
  void ResetFromCurrentBrowsers();

  // Associates a tab with a window.
  void SetTabWindow(const SessionID& window_id, const SessionID& tab_id);

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

  // Note: this is invoked from the NavigationController's destructor, which is
  // after the actual tab has been removed.
  // TODO(stahon@microsoft.com) This should become pure virtual when
  // AppSessionService is implemented because SessionService overrides this.
  virtual void TabClosed(const SessionID& window_id, const SessionID& tab_id);

  // Notification a window has opened.
  virtual void WindowOpened(Browser* browser) = 0;

  // Notification the window is about to close.
  virtual void WindowClosing(const SessionID& window_id) = 0;

  // Notification a window has finished closing.
  virtual void WindowClosed(const SessionID& window_id) = 0;

  // Called when a tab is inserted.
  void TabInserted(content::WebContents* contents);

  // Called when a tab is closing.
  void TabClosing(content::WebContents* contents);

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (ShouldRestoreOfWindowType returns true).
  virtual void SetWindowType(const SessionID& window_id,
                             Browser::Type type) = 0;

  // Sets the index of the selected tab in the specified window.
  void SetSelectedTabInWindow(const SessionID& window_id, int index);

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
  void GetLastSession(sessions::GetLastSessionCallback callback);

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;
  // TODO(stahon@microsoft.com) When AppSessionService is implemented, this
  // can become pure virtual and the implementation move to AppSessionService.
  void OnErrorWritingSessionCommands() override;

  // sessions::SessionTabHelperDelegate:
  void SetTabUserAgentOverride(const SessionID& window_id,
                               const SessionID& tab_id,
                               const sessions::SerializedUserAgentOverride&
                                   user_agent_override) override;
  void SetSelectedNavigationIndex(const SessionID& window_id,
                                  const SessionID& tab_id,
                                  int index) override;
  void UpdateTabNavigation(
      const SessionID& window_id,
      const SessionID& tab_id,
      const sessions::SerializedNavigationEntry& navigation) override;
  void TabNavigationPathPruned(const SessionID& window_id,
                               const SessionID& tab_id,
                               int index,
                               int count) override;
  void TabNavigationPathEntriesDeleted(const SessionID& window_id,
                                       const SessionID& tab_id) override;

 protected:
  // Creates a SessionService for the specified profile.
  SessionServiceBase(Profile* profile, SessionServiceType type);

  bool rebuild_on_next_save() const { return rebuild_on_next_save_; }
  void set_rebuild_on_next_save(bool value) { rebuild_on_next_save_ = value; }
  std::map<SessionID, int>* last_selected_tab_in_window() {
    return &last_selected_tab_in_window_;
  }

  using IdToRange = std::map<SessionID, std::pair<int, int>>;
  IdToRange* tab_to_available_range() { return &tab_to_available_range_; }

  sessions::CommandStorageManager* command_storage_manager() {
    return command_storage_manager_.get();
  }

  // This should only be used by derived classes in their destructors.
  void DestroyCommandStorageManager();

  // Returns true if a window of given |window_type| should get
  // restored upon session restore.
  virtual bool ShouldRestoreWindowOfType(
      sessions::SessionWindow::WindowType type) const = 0;

  // Removes unrestorable windows from the previous windows list.
  void RemoveUnusedRestoreWindows(
      std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list);

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {}
  void OnBrowserSetLastActive(Browser* browser) override;

  // Converts |commands| to SessionWindows and notifies the callback.
  void OnGotSessionCommands(
      sessions::GetLastSessionCallback callback,
      std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
      bool read_error);

  // Adds commands to commands that will recreate the state of the specified
  // tab. This adds at most kMaxNavigationCountToPersist navigations (in each
  // direction from the current navigation index).
  // A pair is added to tab_to_available_range indicating the range of
  // indices that were written.
  virtual void BuildCommandsForTab(const SessionID& window_id,
                                   content::WebContents* tab,
                                   int index_in_window,
                                   base::Optional<tab_groups::TabGroupId> group,
                                   bool is_pinned,
                                   IdToRange* tab_to_available_range) = 0;

  // Adds commands to create the specified browser, and invokes
  // BuildCommandsForTab for each of the tabs in the browser. This ignores
  // any tabs not in the profile we were created with.
  virtual void BuildCommandsForBrowser(
      Browser* browser,
      IdToRange* tab_to_available_range,
      std::set<SessionID>* windows_to_track) = 0;

  // Iterates over all the known browsers invoking BuildCommandsForBrowser.
  // This only adds browsers that should be tracked (|ShouldRestoreWindowOfType|
  // returns true). All browsers that are tracked are added to windows_to_track
  // (as long as it is non-null).
  void BuildCommandsFromBrowsers(IdToRange* tab_to_available_range,
                                 std::set<SessionID>* windows_to_track);

  // Schedules a reset of the existing commands. A reset means the contents
  // of the file are recreated from the state of the browser.
  virtual void ScheduleResetCommands() = 0;

  // Schedules the specified command.
  void ScheduleCommand(std::unique_ptr<sessions::SessionCommand> command);

  // Returns true if changes to tabs in the specified window should be tracked.
  // TODO(stahon@microsoft.com) This should be pure virtual when
  // AppSessionService is implemented.
  virtual bool ShouldTrackChangesToWindow(const SessionID& window_id) const;

  // Returns true if we track changes to the specified browser.
  virtual bool ShouldTrackBrowser(Browser* browser) const = 0;

  // Will rebuild session commands if rebuild_on_next_save_ is true.
  // TODO(stahon@microsoft.com) This should be made pure virtual when
  // AppSessionService is implemented.
  void RebuildCommandsIfRequired();

  // Deletes session data if no windows are open for the current profile.
  virtual void MaybeDeleteSessionOnlyData() = 0;

  // Unit test accessors.
  sessions::CommandStorageManager* GetCommandStorageManagerForTest();

  void SetAvailableRangeForTest(const SessionID& tab_id,
                                const std::pair<int, int>& range);
  bool GetAvailableRangeForTest(const SessionID& tab_id,
                                std::pair<int, int>* range);

 private:
  // This is always non-null.
  Profile* profile_;

  // Whether to use delayed save. Set to false when constructed with a FilePath
  // (which should only be used for testing).
  bool should_use_delayed_save_ = true;

  std::unique_ptr<sessions::CommandStorageManager> command_storage_manager_;

  // Maps from session tab id to the range of navigation entries that has
  // been written to disk.
  //
  // This is only used if not all the navigation entries have been
  // written.
  IdToRange tab_to_available_range_;

  // Force session commands to be rebuild before next save event.
  bool rebuild_on_next_save_ = false;

  // Don't send duplicate SetSelectedTabInWindow commands when the selected
  // tab's index hasn't changed.
  std::map<SessionID, int> last_selected_tab_in_window_;

  base::WeakPtrFactory<SessionServiceBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionServiceBase);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_
