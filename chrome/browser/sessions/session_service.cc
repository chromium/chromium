// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/app_controller_mac.h"
#endif

using base::Time;
using content::NavigationEntry;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

// Every kWritesPerReset commands triggers recreating the file.
static const int kWritesPerReset = 250;

// SessionService -------------------------------------------------------------

SessionService::SessionService(Profile* profile)
    : profile_(profile),
      should_use_delayed_save_(true),
      base_session_service_(new sessions::BaseSessionService(
          sessions::BaseSessionService::SESSION_RESTORE,
          profile->GetPath(),
          this)),
      has_open_trackable_browsers_(false),
      move_on_new_browser_(false),
      force_browser_not_alive_with_no_windows_(false),
      rebuild_on_next_save_(false) {
  // We should never be created when incognito.
  DCHECK(!profile->IsOffTheRecord());
  Init();
}

SessionService::SessionService(const base::FilePath& save_path)
    : profile_(NULL),
      should_use_delayed_save_(false),
      base_session_service_(new sessions::BaseSessionService(
          sessions::BaseSessionService::SESSION_RESTORE,
          save_path,
          this)),
      has_open_trackable_browsers_(false),
      move_on_new_browser_(false),
      force_browser_not_alive_with_no_windows_(false),
      rebuild_on_next_save_(false) {
  Init();
}

SessionService::~SessionService() {
  // The BrowserList should outlive the SessionService since it's static and
  // the SessionService is a KeyedService.
  BrowserList::RemoveObserver(this);
  base_session_service_->Save();
}

bool SessionService::ShouldNewWindowStartSession() {
  // ChromeOS and OSX have different ideas of application lifetime than
  // the other platforms.
  // On ChromeOS opening a new window should never start a new session.
#if defined(OS_CHROMEOS)
  if (!force_browser_not_alive_with_no_windows_)
    return false;
#endif
  if (!has_open_trackable_browsers_ &&
      !StartupBrowserCreator::InSynchronousProfileLaunch() &&
      !SessionRestore::IsRestoring(profile())
#if defined(OS_MACOSX)
      // On OSX, a new window should not start a new session if it was opened
      // from the dock or the menubar.
      && !app_controller_mac::IsOpeningNewWindow()
#endif  // OS_MACOSX
      ) {
    return true;
  }
  return false;
}

bool SessionService::RestoreIfNecessary(const std::vector<GURL>& urls_to_open) {
  return RestoreIfNecessary(urls_to_open, NULL);
}

void SessionService::ResetFromCurrentBrowsers() {
  ScheduleResetCommands();
}

void SessionService::MoveCurrentSessionToLastSession() {
  pending_tab_close_ids_.clear();
  window_closing_ids_.clear();
  pending_window_close_ids_.clear();

  base_session_service_->MoveCurrentSessionToLastSession();
}

void SessionService::DeleteLastSession() {
  base_session_service_->DeleteLastSession();
}

void SessionService::SetTabWindow(const SessionID& window_id,
                                  const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetTabWindowCommand(window_id, tab_id));
}

void SessionService::SetWindowBounds(const SessionID& window_id,
                                     const gfx::Rect& bounds,
                                     ui::WindowShowState show_state) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowBoundsCommand(window_id, bounds, show_state));
}

void SessionService::SetWindowWorkspace(const SessionID& window_id,
                                        const std::string& workspace) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowWorkspaceCommand(window_id, workspace));
}

void SessionService::SetTabIndexInWindow(const SessionID& window_id,
                                         const SessionID& tab_id,
                                         int new_index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabIndexInWindowCommand(tab_id, new_index));
}

void SessionService::SetTabGroup(const SessionID& window_id,
                                 const SessionID& tab_id,
                                 base::Optional<base::Token> group) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Tabs get ungrouped as they close. However, if the whole window is closing
  // tabs should stay in their groups. So, ignore this call in that case.
  if (base::Contains(pending_window_close_ids_, window_id) ||
      base::Contains(window_closing_ids_, window_id))
    return;

  ScheduleCommand(sessions::CreateTabGroupCommand(tab_id, group));
}

void SessionService::SetTabGroupMetadata(const SessionID& window_id,
                                         const base::Token& group_id,
                                         const base::string16& title,
                                         SkColor color) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Any group metadata changes happening in a closing window can be ignored.
  if (base::Contains(pending_window_close_ids_, window_id) ||
      base::Contains(window_closing_ids_, window_id))
    return;

  ScheduleCommand(
      sessions::CreateTabGroupMetadataUpdateCommand(group_id, title, color));
}

void SessionService::SetPinnedState(const SessionID& window_id,
                                    const SessionID& tab_id,
                                    bool is_pinned) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreatePinnedStateCommand(tab_id, is_pinned));
}

void SessionService::TabClosed(const SessionID& window_id,
                               const SessionID& tab_id,
                               bool closed_by_user_gesture) {
  if (!tab_id.id())
    return;  // Hapens when the tab is replaced.

  if (!ShouldTrackChangesToWindow(window_id))
    return;

  auto i = tab_to_available_range_.find(tab_id);
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);

  if (find(pending_window_close_ids_.begin(), pending_window_close_ids_.end(),
           window_id) != pending_window_close_ids_.end()) {
    // Tab is in last window. Don't commit it immediately, instead add it to the
    // list of tabs to close. If the user creates another window, the close is
    // committed.
    pending_tab_close_ids_.insert(tab_id);
  } else if (find(window_closing_ids_.begin(), window_closing_ids_.end(),
                  window_id) != window_closing_ids_.end() ||
             !IsOnlyOneTabLeft() || closed_by_user_gesture) {
    // Close is the result of one of the following:
    // . window close (and it isn't the last window).
    // . closing a tab and there are other windows/tabs open.
    // . closed by a user gesture.
    // In all cases we need to mark the tab as explicitly closed.
    ScheduleCommand(sessions::CreateTabClosedCommand(tab_id));
  } else {
    // User closed the last tab in the last tabbed browser. Don't mark the
    // tab closed.
    pending_tab_close_ids_.insert(tab_id);
    has_open_trackable_browsers_ = false;
  }
}

void SessionService::WindowOpened(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  RestoreIfNecessary(std::vector<GURL>(), browser);
  SetWindowType(browser->session_id(), browser->type());
  SetWindowAppName(browser->session_id(), browser->app_name());
}

void SessionService::WindowClosing(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // If Chrome is closed immediately after a history deletion, we have to
  // rebuild commands before this window is closed, otherwise these tabs would
  // be lost.
  RebuildCommandsIfRequired();

  // The window is about to close. If there are other tabbed browsers with the
  // same original profile commit the close immediately.
  //
  // NOTE: if the user chooses the exit menu item session service is destroyed
  // and this code isn't hit.
  if (has_open_trackable_browsers_) {
    // Closing a window can never make has_open_trackable_browsers_ go from
    // false to true, so only update it if already true.
    has_open_trackable_browsers_ = HasOpenTrackableBrowsers(window_id);
  }
  bool use_pending_close = !has_open_trackable_browsers_;
  if (!use_pending_close) {
    // Somewhat outside of "normal behavior" is profile locking.  In this case
    // (when IsSiginRequired has already been set True), we're closing all
    // browser windows in turn but want them all to be restored when the user
    // unlocks.  To accomplish this, we do a "pending close" on all windows
    // instead of just the last one (which has no open_trackable_browsers).
    // http://crbug.com/356818
    //
    // Some editions (like iOS) don't have a profile_manager and some tests
    // don't supply one so be lenient.
    if (g_browser_process) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      if (profile_manager) {
        ProfileAttributesEntry* entry;
        bool has_entry = profile_manager->GetProfileAttributesStorage().
            GetProfileAttributesWithPath(profile()->GetPath(), &entry);
        use_pending_close = has_entry && entry->IsSigninRequired();
      }
    }
  }
  if (use_pending_close)
    pending_window_close_ids_.insert(window_id);
  else
    window_closing_ids_.insert(window_id);
}

void SessionService::WindowClosed(const SessionID& window_id) {
  if (!ShouldTrackChangesToWindow(window_id)) {
    // The last window may be one that is not tracked.
    MaybeDeleteSessionOnlyData();
    return;
  }

  windows_tracking_.erase(window_id);
  last_selected_tab_in_window_.erase(window_id);

  if (window_closing_ids_.find(window_id) != window_closing_ids_.end()) {
    window_closing_ids_.erase(window_id);
    ScheduleCommand(sessions::CreateWindowClosedCommand(window_id));
  } else if (pending_window_close_ids_.find(window_id) ==
             pending_window_close_ids_.end()) {
    // We'll hit this if user closed the last tab in a window.
    has_open_trackable_browsers_ = HasOpenTrackableBrowsers(window_id);
    if (!has_open_trackable_browsers_)
      pending_window_close_ids_.insert(window_id);
    else
      ScheduleCommand(sessions::CreateWindowClosedCommand(window_id));
  }
  MaybeDeleteSessionOnlyData();
}

void SessionService::TabInserted(WebContents* contents) {
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(contents);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;
  SetTabWindow(session_tab_helper->window_id(),
               session_tab_helper->session_id());
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (extensions_tab_helper && extensions_tab_helper->is_app()) {
    SetTabExtensionAppID(session_tab_helper->window_id(),
                         session_tab_helper->session_id(),
                         extensions_tab_helper->GetAppId());
  }

  // Record the association between the SessionStorageNamespace and the
  // tab.
  //
  // TODO(ajwong): This should be processing the whole map rather than
  // just the default. This in particular will not work for tabs with only
  // isolated apps which won't have a default partition.
  content::SessionStorageNamespace* session_storage_namespace =
      contents->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(), session_storage_namespace->id()));
  session_storage_namespace->SetShouldPersist(true);
}

void SessionService::TabClosing(WebContents* contents) {
  // Allow the associated sessionStorage to get deleted; it won't be needed
  // in the session restore.
  content::SessionStorageNamespace* session_storage_namespace =
      contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(false);
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(contents);
  TabClosed(session_tab_helper->window_id(),
            session_tab_helper->session_id(),
            contents->GetClosedByUserGesture());
}

void SessionService::SetWindowType(const SessionID& window_id,
                                   Browser::Type type) {
  sessions::SessionWindow::WindowType window_type =
      WindowTypeForBrowserType(type);
  if (!ShouldRestoreWindowOfType(window_type))
    return;

  windows_tracking_.insert(window_id);

  // The user created a new tabbed browser with our profile. Commit any
  // pending closes.
  CommitPendingCloses();

  has_open_trackable_browsers_ = true;
  move_on_new_browser_ = true;

  ScheduleCommand(CreateSetWindowTypeCommand(window_id, window_type));
}

void SessionService::SetWindowAppName(
    const SessionID& window_id,
    const std::string& app_name) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetWindowAppNameCommand(window_id, app_name));
}

void SessionService::TabNavigationPathPruned(const SessionID& window_id,
                                             const SessionID& tab_id,
                                             int index,
                                             int count) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  DCHECK_GE(index, 0);
  DCHECK_GT(count, 0);

  // Update the range of available indices.
  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];

    // if both range.first and range.second are also deleted.
    if (range.second >= index && range.second < index + count &&
        range.first >= index && range.first < index + count) {
      range.first = range.second = 0;
    } else {
      // Update range.first
      if (range.first >= index + count)
        range.first = range.first - count;
      else if (range.first >= index && range.first < index + count)
        range.first = index;

      // Update range.second
      if (range.second >= index + count)
        range.second = std::max(range.first, range.second - count);
      else if (range.second >= index && range.second < index + count)
        range.second = std::max(range.first, index - 1);
    }
  }

  return ScheduleCommand(
      sessions::CreateTabNavigationPathPrunedCommand(tab_id, index, count));
}

void SessionService::TabNavigationPathEntriesDeleted(const SessionID& window_id,
                                                     const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Multiple tabs might be affected by this deletion, so the rebuild is
  // delayed until next save.
  rebuild_on_next_save_ = true;
  base_session_service_->StartSaveTimer();
}

void SessionService::UpdateTabNavigation(
    const SessionID& window_id,
    const SessionID& tab_id,
    const SerializedNavigationEntry& navigation) {
  if (!ShouldTrackURLForRestore(navigation.virtual_url()) ||
      !ShouldTrackChangesToWindow(window_id)) {
    return;
  }

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    std::pair<int, int>& range = tab_to_available_range_[tab_id];
    range.first = std::min(navigation.index(), range.first);
    range.second = std::max(navigation.index(), range.second);
  }
  ScheduleCommand(CreateUpdateTabNavigationCommand(tab_id, navigation));
}

void SessionService::TabRestored(WebContents* tab, bool pinned) {
  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(tab);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;

  // TODO(crbug.com/930991): handle tab groups here.
  BuildCommandsForTab(session_tab_helper->window_id(), tab, -1, base::nullopt,
                      pinned, NULL);
  base_session_service_->StartSaveTimer();
}

void SessionService::SetSelectedNavigationIndex(const SessionID& window_id,
                                                const SessionID& tab_id,
                                                int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  if (tab_to_available_range_.find(tab_id) != tab_to_available_range_.end()) {
    if (index < tab_to_available_range_[tab_id].first ||
        index > tab_to_available_range_[tab_id].second) {
      // The new index is outside the range of what we've archived, schedule
      // a reset.
      ResetFromCurrentBrowsers();
      return;
    }
  }
  ScheduleCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(tab_id, index));
}

void SessionService::SetSelectedTabInWindow(const SessionID& window_id,
                                            int index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  auto it = last_selected_tab_in_window_.find(window_id);
  if (it != last_selected_tab_in_window_.end() && it->second == index)
    return;
  last_selected_tab_in_window_[window_id] = index;

  ScheduleCommand(
      sessions::CreateSetSelectedTabInWindowCommand(window_id, index));
}

void SessionService::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& user_agent_override) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
      tab_id, user_agent_override));
}

void SessionService::SetTabExtensionAppID(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& extension_app_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabExtensionAppIDCommand(tab_id, extension_app_id));
}

void SessionService::SetLastActiveTime(const SessionID& window_id,
                                       const SessionID& tab_id,
                                       base::TimeTicks last_active_time) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateLastActiveTimeCommand(tab_id, last_active_time));
}

base::CancelableTaskTracker::TaskId SessionService::GetLastSession(
    const sessions::GetLastSessionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // OnGotSessionCommands maps the SessionCommands to browser state, then run
  // the callback.
  return base_session_service_->ScheduleGetLastSessionCommands(
      base::Bind(&SessionService::OnGotSessionCommands,
                 weak_factory_.GetWeakPtr(),
                 callback),
      tracker);
}

bool SessionService::ShouldUseDelayedSave() {
  return should_use_delayed_save_;
}

void SessionService::OnWillSaveCommands() {
  RebuildCommandsIfRequired();
}

void SessionService::RebuildCommandsIfRequired() {
  if (rebuild_on_next_save_ && pending_window_close_ids_.empty()) {
    ScheduleResetCommands();
  }
}

void SessionService::Init() {
  BrowserList::AddObserver(this);
}

bool SessionService::ShouldRestoreWindowOfType(
    sessions::SessionWindow::WindowType window_type) const {
#if defined(OS_CHROMEOS)
  // Restore app popups for ChromeOS alone.
  if (window_type == sessions::SessionWindow::TYPE_APP)
    return true;
#endif

  return (window_type == sessions::SessionWindow::TYPE_NORMAL) ||
         (window_type == sessions::SessionWindow::TYPE_POPUP);
}

void SessionService::RemoveUnusedRestoreWindows(
    std::vector<std::unique_ptr<sessions::SessionWindow>>* window_list) {
  auto i = window_list->begin();
  while (i != window_list->end()) {
    sessions::SessionWindow* window = i->get();
    if (!ShouldRestoreWindowOfType(window->type)) {
      i = window_list->erase(i);
    } else {
      ++i;
    }
  }
}

bool SessionService::RestoreIfNecessary(const std::vector<GURL>& urls_to_open,
                                        Browser* browser) {
  if (ShouldNewWindowStartSession()) {
    // We're going from no tabbed browsers to a tabbed browser (and not in
    // process startup), restore the last session.
    if (move_on_new_browser_) {
      // Make the current session the last.
      MoveCurrentSessionToLastSession();
      move_on_new_browser_ = false;
    }
    SessionStartupPref pref = StartupBrowserCreator::GetSessionStartupPref(
        *base::CommandLine::ForCurrentProcess(), profile());
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfileIfExisting(profile());
    if (pref.type == SessionStartupPref::LAST &&
        (!tab_restore_service || !tab_restore_service->IsRestoring())) {
      SessionRestore::RestoreSession(
          profile(), browser,
          browser ? 0 : SessionRestore::ALWAYS_CREATE_TABBED_BROWSER,
          urls_to_open);
      return true;
    }
  }
  return false;
}

void SessionService::OnBrowserSetLastActive(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    ScheduleCommand(
        sessions::CreateSetActiveWindowCommand(browser->session_id()));
}

void SessionService::OnGotSessionCommands(
    const sessions::GetLastSessionCallback& callback,
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands) {
  std::vector<std::unique_ptr<sessions::SessionWindow>> valid_windows;
  SessionID active_window_id = SessionID::InvalidValue();

  sessions::RestoreSessionFromCommands(commands, &valid_windows,
                                       &active_window_id);
  RemoveUnusedRestoreWindows(&valid_windows);

  callback.Run(std::move(valid_windows), active_window_id);
}

void SessionService::BuildCommandsForTab(const SessionID& window_id,
                                         WebContents* tab,
                                         int index_in_window,
                                         base::Optional<base::Token> group,
                                         bool is_pinned,
                                         IdToRange* tab_to_available_range) {
  DCHECK(tab);
  DCHECK(window_id.is_valid());

  SessionTabHelper* session_tab_helper = SessionTabHelper::FromWebContents(tab);
  const SessionID& session_id(session_tab_helper->session_id());
  base_session_service_->AppendRebuildCommand(
      sessions::CreateSetTabWindowCommand(window_id, session_id));

  const int current_index = tab->GetController().GetCurrentEntryIndex();
  const int min_index =
      std::max(current_index - sessions::gMaxPersistNavigationCount, 0);
  const int max_index =
      std::min(current_index + sessions::gMaxPersistNavigationCount,
               tab->GetController().GetEntryCount());
  const int pending_index = tab->GetController().GetPendingEntryIndex();
  if (tab_to_available_range) {
    (*tab_to_available_range)[session_id] =
        std::pair<int, int>(min_index, max_index);
  }

  if (is_pinned) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreatePinnedStateCommand(session_id, true));
  }

  base_session_service_->AppendRebuildCommand(
      sessions::CreateLastActiveTimeCommand(session_id,
                                            tab->GetLastActiveTime()));

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(tab);
  if (extensions_tab_helper->is_app()) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreateSetTabExtensionAppIDCommand(
            session_id, extensions_tab_helper->GetAppId()));
  }

  const std::string& ua_override = tab->GetUserAgentOverride();
  if (!ua_override.empty()) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreateSetTabUserAgentOverrideCommand(session_id,
                                                       ua_override));
  }

  for (int i = min_index; i < max_index; ++i) {
    NavigationEntry* entry = (i == pending_index)
                                 ? tab->GetController().GetPendingEntry()
                                 : tab->GetController().GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackURLForRestore(entry->GetVirtualURL())) {
      const SerializedNavigationEntry navigation =
          ContentSerializedNavigationBuilder::FromNavigationEntry(i, entry);
      base_session_service_->AppendRebuildCommand(
          CreateUpdateTabNavigationCommand(session_id, navigation));
    }
  }
  base_session_service_->AppendRebuildCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(session_id,
                                                        current_index));

  if (index_in_window != -1) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreateSetTabIndexInWindowCommand(session_id,
                                                   index_in_window));
  }

  if (group.has_value()) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreateTabGroupCommand(session_id, group));
  }

  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      tab->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(), session_storage_namespace->id()));
}

void SessionService::BuildCommandsForBrowser(
    Browser* browser,
    IdToRange* tab_to_available_range,
    std::set<SessionID>* windows_to_track) {
  DCHECK(browser);
  DCHECK(browser->session_id().is_valid());

  base_session_service_->AppendRebuildCommand(
      sessions::CreateSetWindowBoundsCommand(
          browser->session_id(),
          browser->window()->GetRestoredBounds(),
          browser->window()->GetRestoredState()));

  base_session_service_->AppendRebuildCommand(
      sessions::CreateSetWindowTypeCommand(
          browser->session_id(),
          WindowTypeForBrowserType(browser->type())));

  if (!browser->app_name().empty()) {
    base_session_service_->AppendRebuildCommand(
        sessions::CreateSetWindowAppNameCommand(
            browser->session_id(),
            browser->app_name()));
  }

  sessions::CreateSetWindowWorkspaceCommand(
      browser->session_id(), browser->window()->GetWorkspace());

  windows_to_track->insert(browser->session_id());
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    const base::Optional<TabGroupId> group_id = tab_strip->GetTabGroupForTab(i);
    const base::Optional<base::Token> raw_group_id =
        group_id.has_value() ? base::make_optional(group_id.value().token())
                             : base::nullopt;
    BuildCommandsForTab(browser->session_id(), tab, i, raw_group_id,
                        tab_strip->IsTabPinned(i), tab_to_available_range);
  }

  // Set the visual data for each tab group.
  for (const TabGroupId& group_id : tab_strip->ListTabGroups()) {
    const TabGroupVisualData* data = tab_strip->GetVisualDataForGroup(group_id);
    base_session_service_->AppendRebuildCommand(
        sessions::CreateTabGroupMetadataUpdateCommand(
            group_id.token(), data->title(), data->color()));
  }

  base_session_service_->AppendRebuildCommand(
      sessions::CreateSetSelectedTabInWindowCommand(
          browser->session_id(),
          browser->tab_strip_model()->active_index()));
}

void SessionService::BuildCommandsFromBrowsers(
    IdToRange* tab_to_available_range,
    std::set<SessionID>* windows_to_track) {
  for (auto* browser : *BrowserList::GetInstance()) {
    // Make sure the browser has tabs and a window. Browser's destructor
    // removes itself from the BrowserList. When a browser is closed the
    // destructor is not necessarily run immediately. This means it's possible
    // for us to get a handle to a browser that is about to be removed. If
    // the tab count is 0 or the window is NULL, the browser is about to be
    // deleted, so we ignore it.
    if (ShouldTrackBrowser(browser) && browser->tab_strip_model()->count() &&
        browser->window()) {
      BuildCommandsForBrowser(browser,
                              tab_to_available_range,
                              windows_to_track);
    }
  }
}

void SessionService::ScheduleResetCommands() {
  base_session_service_->set_pending_reset(true);
  base_session_service_->ClearPendingCommands();
  tab_to_available_range_.clear();
  windows_tracking_.clear();
  last_selected_tab_in_window_.clear();
  rebuild_on_next_save_ = false;
  BuildCommandsFromBrowsers(&tab_to_available_range_,
                            &windows_tracking_);
  if (!windows_tracking_.empty()) {
    // We're lazily created on startup and won't get an initial batch of
    // SetWindowType messages. Set these here to make sure our state is correct.
    has_open_trackable_browsers_ = true;
    move_on_new_browser_ = true;
  }
  base_session_service_->StartSaveTimer();
}

void SessionService::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  if (ReplacePendingCommand(base_session_service_.get(), &command))
    return;
  bool is_closing_command = IsClosingCommand(command.get());
  base_session_service_->ScheduleCommand(std::move(command));
  // Don't schedule a reset on tab closed/window closed. Otherwise we may
  // lose tabs/windows we want to restore from if we exit right after this.
  if (!base_session_service_->pending_reset() &&
      pending_window_close_ids_.empty() &&
      base_session_service_->commands_since_reset() >= kWritesPerReset &&
      !is_closing_command) {
    ScheduleResetCommands();
  }
}

void SessionService::CommitPendingCloses() {
  for (auto i = pending_tab_close_ids_.begin();
       i != pending_tab_close_ids_.end(); ++i) {
    ScheduleCommand(sessions::CreateTabClosedCommand(*i));
  }
  pending_tab_close_ids_.clear();

  for (auto i = pending_window_close_ids_.begin();
       i != pending_window_close_ids_.end(); ++i) {
    ScheduleCommand(sessions::CreateWindowClosedCommand(*i));
  }
  pending_window_close_ids_.clear();
}

bool SessionService::IsOnlyOneTabLeft() const {
  if (!profile() || profile()->AsTestingProfile()) {
    // We're testing, always return false.
    return false;
  }

  int window_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    const SessionID window_id = browser->session_id();
    if (ShouldTrackBrowser(browser) &&
        window_closing_ids_.find(window_id) == window_closing_ids_.end()) {
      if (++window_count > 1)
        return false;
      // By the time this is invoked the tab has been removed. As such, we use
      // > 0 here rather than > 1.
      if (browser->tab_strip_model()->count() > 0)
        return false;
    }
  }
  return true;
}

bool SessionService::HasOpenTrackableBrowsers(
    const SessionID& window_id) const {
  if (!profile() || profile()->AsTestingProfile()) {
    // We're testing, always return true.
    return true;
  }

  for (auto* browser : *BrowserList::GetInstance()) {
    const SessionID browser_id = browser->session_id();
    if (browser_id != window_id &&
        window_closing_ids_.find(browser_id) == window_closing_ids_.end() &&
        ShouldTrackBrowser(browser)) {
      return true;
    }
  }
  return false;
}

bool SessionService::ShouldTrackChangesToWindow(
    const SessionID& window_id) const {
  return windows_tracking_.find(window_id) != windows_tracking_.end();
}

bool SessionService::ShouldTrackBrowser(Browser* browser) const {
  if (browser->profile() != profile())
    return false;
#if defined(OS_CHROMEOS)
  // Do not track Crostini apps because they will be dead upon restoring the
  // browser session since VMs do not automatically restart on session restore.
  if (crostini::CrostiniAppIdFromAppName(browser->app_name())) {
    return false;
  }
#endif
  // Never track app popup windows that do not have a trusted source (i.e.
  // popup windows spawned by an app). If this logic changes, be sure to also
  // change SessionRestoreImpl::CreateRestoredBrowser().
  if (browser->deprecated_is_app() && !browser->is_trusted_source()) {
    return false;
  }
  return ShouldRestoreWindowOfType(WindowTypeForBrowserType(browser->type()));
}

void SessionService::MaybeDeleteSessionOnlyData() {
  // Don't try anything if we're testing.  The browser_process is not fully
  // created and DeleteSession will crash if we actually attempt it.
  if (!profile() || profile()->AsTestingProfile())
    return;

  // Clear session data if the last window for a profile has been closed and
  // closing the last window would normally close Chrome, unless background mode
  // is active.  Tests don't have a background_mode_manager.
  if (has_open_trackable_browsers_ ||
      browser_defaults::kBrowserAliveWithNoWindows ||
      g_browser_process->background_mode_manager()->IsBackgroundModeActive()) {
    return;
  }

  // Check for any open windows for the current profile that we aren't tracking.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile())
      return;
  }
  DeleteSessionOnlyData(profile());
}

sessions::BaseSessionService* SessionService::GetBaseSessionServiceForTest() {
  return base_session_service_.get();
}

void SessionService::SetAvailableRangeForTest(
    const SessionID& tab_id,
    const std::pair<int, int>& range) {
  tab_to_available_range_[tab_id] = range;
}

bool SessionService::GetAvailableRangeForTest(const SessionID& tab_id,
                                              std::pair<int, int>* range) {
  auto i = tab_to_available_range_.find(tab_id);
  if (i == tab_to_available_range_.end())
    return false;

  *range = i->second;
  return true;
}
