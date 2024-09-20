// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
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
 public:
  enum class SessionServiceType { kAppRestore, kSessionRestore };

  SessionServiceBase(const SessionServiceBase&) = delete;
  SessionServiceBase& operator=(const SessionServiceBase&) = delete;

  ~SessionServiceBase() override;

  static Browser::Type GetBrowserTypeFromWebContents(
      content::WebContents* web_contents);

  Profile* profile() const { return profile_; }

  bool is_saving_enabled() const { return is_saving_enabled_; }

  // Sets whether the window is visible on all workspaces or not.
  void SetWindowVisibleOnAllWorkspaces(SessionID window_id,
                                       bool visible_on_all_workspaces);

  // Resets the contents of the file from the current state of all open
  // apps whose profile matches our profile.
  void ResetFromCurrentBrowsers();

  // Associates a tab with a window.
  void SetTabWindow(SessionID window_id, SessionID tab_id);

  // Sets the bounds of a window.
  void SetWindowBounds(SessionID window_id,
                       const gfx::Rect& bounds,
                       ui::mojom::WindowShowState show_state);

  // Sets the workspace the window resides in.
  void SetWindowWorkspace(SessionID window_id, const std::string& workspace);

  // Sets the visual index of the tab in its parent window.
  void SetTabIndexInWindow(SessionID window_id,
                           SessionID tab_id,
                           int new_index);

  // Note: this is invoked from the NavigationController's destructor, which is
  // after the actual tab has been removed.
  virtual void TabClosed(SessionID window_id, SessionID tab_id) = 0;

  // Notification a window has opened.
  virtual void WindowOpened(Browser* browser) = 0;

  // Notification the window is about to close.
  virtual void WindowClosing(SessionID window_id) = 0;

  // Notification a window has finished closing.
  virtual void WindowClosed(SessionID window_id) = 0;

  // Called when a tab is inserted.
  void TabInserted(content::WebContents* contents);

  // Called when a tab is closing.
  void TabClosing(content::WebContents* contents);

  // Notification that a tab has restored its entries or a closed tab is being
  // reused.
  void TabRestored(content::WebContents* tab, bool pinned);

  // Sets the type of window. In order for the contents of a window to be
  // tracked SetWindowType must be invoked with a type we track
  // (ShouldRestoreOfWindowType returns true).
  virtual void SetWindowType(SessionID window_id, Browser::Type type) = 0;

  // Sets the index of the selected tab in the specified window.
  void SetSelectedTabInWindow(SessionID window_id, int index);

  // Sets the application extension id of the specified tab.
  void SetTabExtensionAppID(SessionID window_id,
                            SessionID tab_id,
                            const std::string& extension_app_id);

  // Sets the last active time of the tab.
  void SetLastActiveTime(SessionID window_id,
                         SessionID tab_id,
                         base::Time last_active_time);

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  void GetLastSession(sessions::GetLastSessionCallback callback);

  // Sets the application name of the specified window.
  void SetWindowAppName(SessionID window_id, const std::string& app_name);

  // Sets the pinned state of the tab.
  void SetPinnedState(SessionID window_id, SessionID tab_id, bool is_pinned);

  // CommandStorageManagerDelegate:
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override;
  // This implementation in SessionServiceBase is mostly a no-op.
  // Full support for Session Service logging will come with
  // https://crbug.com/1193711
  void OnErrorWritingSessionCommands() override;

  // sessions::SessionTabHelperDelegate:
  void SetTabUserAgentOverride(SessionID window_id,
                               SessionID tab_id,
                               const sessions::SerializedUserAgentOverride&
                                   user_agent_override) override;
  void SetSelectedNavigationIndex(SessionID window_id,
                                  SessionID tab_id,
                                  int index) override;
  void UpdateTabNavigation(
      SessionID window_id,
      SessionID tab_id,
      const sessions::SerializedNavigationEntry& navigation) override;
  void TabNavigationPathPruned(SessionID window_id,
                               SessionID tab_id,
                               int index,
                               int count) override;
  void TabNavigationPathEntriesDeleted(SessionID window_id,
                                       SessionID tab_id) override;

 protected:
  // Creates a SessionService for the specified profile.
  SessionServiceBase(Profile* profile, SessionServiceType type);

  // This method is implemented by child classes to pass us the type.
  virtual Browser::Type GetDesiredBrowserTypeForWebContents() = 0;

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

  using WindowsTracking = std::set<SessionID>;
  WindowsTracking* windows_tracking() { return &windows_tracking_; }

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
  virtual void BuildCommandsForTab(SessionID window_id,
                                   content::WebContents* tab,
                                   int index_in_window,
                                   std::optional<tab_groups::TabGroupId> group,
                                   bool is_pinned,
                                   IdToRange* tab_to_available_range);

  // Adds commands to create the specified browser, and invokes
  // BuildCommandsForTab for each of the tabs in the browser. This ignores
  // any tabs not in the profile we were created with.
  virtual void BuildCommandsForBrowser(Browser* browser,
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
  virtual void ScheduleResetCommands() = 0;

  // Schedules the specified command.
  void ScheduleCommand(std::unique_ptr<sessions::SessionCommand> command);

  virtual void DidScheduleCommand() {}

  // Returns true if changes to tabs in the specified window should be tracked.
  bool ShouldTrackChangesToWindow(SessionID window_id) const;

  // Returns true if we track changes to the specified browser.
  bool ShouldTrackBrowser(Browser* browser) const;

  // Will rebuild session commands if rebuild_on_next_save_ is true.
  virtual void RebuildCommandsIfRequired() = 0;

  // Unit test accessors.
  sessions::CommandStorageManager* GetCommandStorageManagerForTest();

  void SetAvailableRangeForTest(SessionID tab_id,
                                const std::pair<int, int>& range);
  bool GetAvailableRangeForTest(SessionID tab_id, std::pair<int, int>* range);

  // Sets whether commands are saved. If false, SessionCommands are effectively
  // dropped (deleted). This is intended for use after a crash to ensure no
  // commands are written before the user acknowledges/restores the crash.
  void SetSavingEnabled(bool enabled);

 private:
  friend class SessionServiceBaseTestHelper;
  friend class SessionServiceTestHelper;

  // This is always non-null.
  raw_ptr<Profile> profile_;

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

  // Set of windows we're tracking changes to. This is only browsers that
  // return true from |ShouldRestoreWindowOfType|.
  WindowsTracking windows_tracking_;

  bool is_saving_enabled_ = true;

  bool did_save_commands_at_least_once_ = false;

  base::WeakPtrFactory<SessionServiceBase> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_SERVICE_BASE_H_
