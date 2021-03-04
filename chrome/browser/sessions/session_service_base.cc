// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_base.h"

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#endif

#if defined(OS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif

using base::Time;
using content::WebContents;
using sessions::SerializedNavigationEntry;

namespace {

// Every kWritesPerReset commands triggers recreating the file.
const int kWritesPerReset = 250;

// User data key for BrowserContextData.
const void* const kProfileTaskRunnerKey = &kProfileTaskRunnerKey;
const void* const kProfileTaskRunnerKeyForApps = &kProfileTaskRunnerKeyForApps;

// Tracks the SequencedTaskRunner that SessionService uses for a particular
// profile. At certain points SessionService may be destroyed, and then
// recreated. This class ensures that when this happens, the same
// SequencedTaskRunner is used. Without this, each instance would have its
// own SequencedTaskRunner, which is problematic as it might then be possible
// for the newly created SessionService to attempt to read from a file the
// previous SessionService was still writing to. No two instances of
// SessionService for a particular profile and type combination can exist at the
// same time, but the backend (CommandStorageBackend) is destroyed on the
// SequencedTaskRunner, meaning without this, it might be possible for two
// CommandStorageBackends to exist at the same time and attempt to use the same
// file.
class TaskRunnerData : public base::SupportsUserData::Data {
 public:
  TaskRunnerData()
      : task_runner_(
            sessions::CommandStorageManager::CreateDefaultBackendTaskRunner()) {
  }
  TaskRunnerData(const TaskRunnerData&) = delete;
  TaskRunnerData& operator=(const TaskRunnerData&) = delete;
  ~TaskRunnerData() override = default;

  static scoped_refptr<base::SequencedTaskRunner>
  GetBackendTaskRunnerForProfile(Profile* profile,
                                 SessionServiceBase::SessionServiceType type) {
    const void* key =
        type == SessionServiceBase::SessionServiceType::kAppRestore
            ? kProfileTaskRunnerKeyForApps
            : kProfileTaskRunnerKey;

    TaskRunnerData* data =
        static_cast<TaskRunnerData*>(profile->GetUserData(key));
    if (!data) {
      profile->SetUserData(key, std::make_unique<TaskRunnerData>());
      data = static_cast<TaskRunnerData*>(profile->GetUserData(key));
    }
    return data->task_runner_;
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

// SessionServiceBase
// -------------------------------------------------------------
// Note: This is protected as it is only intended to be called by child classes
SessionServiceBase::SessionServiceBase(Profile* profile,
                                       SessionServiceType type)
    : profile_(profile) {
  // Covert SessionServiceType to backend CSM::SessionType enum.
  sessions::CommandStorageManager::SessionType backend_type =
      type == SessionServiceType::kSessionRestore
          ? sessions::CommandStorageManager::kSessionRestore
          : sessions::CommandStorageManager::kAppRestore;

  command_storage_manager_ = std::make_unique<sessions::CommandStorageManager>(
      backend_type, profile->GetPath(), this,
      /* use_marker */ true,
      /* enable_crypto */ false, std::vector<uint8_t>(),
      TaskRunnerData::GetBackendTaskRunnerForProfile(profile, type));

  // We should never be created when incognito.
  DCHECK(!profile->IsOffTheRecord());
  BrowserList::AddObserver(this);
}

SessionServiceBase::~SessionServiceBase() {
  // The BrowserList should outlive the SessionService since it's static and
  // the SessionService is a KeyedService.
  BrowserList::RemoveObserver(this);

  // command_storage_manager_->Save() should be called by child classes which
  // should have destructed the command_storage_manager.
  DCHECK(command_storage_manager_ == nullptr);
}

void SessionServiceBase::SetWindowVisibleOnAllWorkspaces(
    const SessionID& window_id,
    bool visible_on_all_workspaces) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetWindowVisibleOnAllWorkspacesCommand(
      window_id, visible_on_all_workspaces));
}

void SessionServiceBase::ResetFromCurrentBrowsers() {
  ScheduleResetCommands();
}

void SessionServiceBase::SetTabWindow(const SessionID& window_id,
                                      const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetTabWindowCommand(window_id, tab_id));
}

void SessionServiceBase::SetWindowBounds(const SessionID& window_id,
                                         const gfx::Rect& bounds,
                                         ui::WindowShowState show_state) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowBoundsCommand(window_id, bounds, show_state));
}

void SessionServiceBase::SetWindowWorkspace(const SessionID& window_id,
                                            const std::string& workspace) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowWorkspaceCommand(window_id, workspace));
}

void SessionServiceBase::SetTabIndexInWindow(const SessionID& window_id,
                                             const SessionID& tab_id,
                                             int new_index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabIndexInWindowCommand(tab_id, new_index));
}

void SessionServiceBase::TabClosed(const SessionID& window_id,
                                   const SessionID& tab_id) {
  if (!tab_id.id())
    return;  // Happens when the tab is replaced.

  if (!ShouldTrackChangesToWindow(window_id))
    return;

  auto i = tab_to_available_range_.find(tab_id);
  if (i != tab_to_available_range_.end())
    tab_to_available_range_.erase(i);

  // If an individual tab is being closed or a secondary window is being
  // closed, just mark the tab as closed now.
  ScheduleCommand(sessions::CreateTabClosedCommand(tab_id));
}

void SessionServiceBase::TabInserted(WebContents* contents) {
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(contents);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;
  SetTabWindow(session_tab_helper->window_id(),
               session_tab_helper->session_id());
  std::string app_id = apps::GetAppIdForWebContents(contents);
  if (!app_id.empty()) {
    SetTabExtensionAppID(session_tab_helper->window_id(),
                         session_tab_helper->session_id(), app_id);
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

void SessionServiceBase::TabClosing(WebContents* contents) {
  // Allow the associated sessionStorage to get deleted; it won't be needed
  // in the session restore.
  content::SessionStorageNamespace* session_storage_namespace =
      contents->GetController().GetDefaultSessionStorageNamespace();
  session_storage_namespace->SetShouldPersist(false);
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(contents);
  TabClosed(session_tab_helper->window_id(), session_tab_helper->session_id());
}

void SessionServiceBase::SetSelectedTabInWindow(const SessionID& window_id,
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

void SessionServiceBase::SetTabExtensionAppID(
    const SessionID& window_id,
    const SessionID& tab_id,
    const std::string& extension_app_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabExtensionAppIDCommand(tab_id, extension_app_id));
}

void SessionServiceBase::SetLastActiveTime(const SessionID& window_id,
                                           const SessionID& tab_id,
                                           base::TimeTicks last_active_time) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateLastActiveTimeCommand(tab_id, last_active_time));
}

void SessionServiceBase::GetLastSession(
    sessions::GetLastSessionCallback callback) {
  // OnGotSessionCommands maps the SessionCommands to browser state, then run
  // the callback.
  return command_storage_manager_->GetLastSessionCommands(
      base::BindOnce(&SessionServiceBase::OnGotSessionCommands,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool SessionServiceBase::ShouldUseDelayedSave() {
  return should_use_delayed_save_;
}

void SessionServiceBase::OnWillSaveCommands() {
  RebuildCommandsIfRequired();
}

void SessionServiceBase::OnErrorWritingSessionCommands() {
  // TODO(stahon@microsoft.com) this is a bit weird to have hardcoded false.
  LogSessionServiceWriteErrorEvent(profile_, false);
  rebuild_on_next_save_ = true;
  RebuildCommandsIfRequired();
}

void SessionServiceBase::SetTabUserAgentOverride(
    const SessionID& window_id,
    const SessionID& tab_id,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  // This is overridden by session_service implementation.
  // We still need it here because we derive from
  // sessions::SessionTabHelperDelegate.
  NOTREACHED();
  return;
}

void SessionServiceBase::SetSelectedNavigationIndex(const SessionID& window_id,
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

void SessionServiceBase::UpdateTabNavigation(
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

void SessionServiceBase::TabNavigationPathPruned(const SessionID& window_id,
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

void SessionServiceBase::TabNavigationPathEntriesDeleted(
    const SessionID& window_id,
    const SessionID& tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Multiple tabs might be affected by this deletion, so the rebuild is
  // delayed until next save.
  rebuild_on_next_save_ = true;
  command_storage_manager_->StartSaveTimer();
}

void SessionServiceBase::DestroyCommandStorageManager() {
  command_storage_manager_.reset(nullptr);
}

void SessionServiceBase::RemoveUnusedRestoreWindows(
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

void SessionServiceBase::OnBrowserSetLastActive(Browser* browser) {
  if (ShouldTrackBrowser(browser))
    ScheduleCommand(
        sessions::CreateSetActiveWindowCommand(browser->session_id()));
}

void SessionServiceBase::OnGotSessionCommands(
    sessions::GetLastSessionCallback callback,
    std::vector<std::unique_ptr<sessions::SessionCommand>> commands,
    bool read_error) {
  // TODO(stahon@microsoft.com) We need to remove unexpected types for
  // AppSessionService related migration purposes.
  std::vector<std::unique_ptr<sessions::SessionWindow>> valid_windows;
  SessionID active_window_id = SessionID::InvalidValue();

  sessions::RestoreSessionFromCommands(commands, &valid_windows,
                                       &active_window_id);
  RemoveUnusedRestoreWindows(&valid_windows);

  std::move(callback).Run(std::move(valid_windows), active_window_id,
                          read_error);
}

void SessionServiceBase::BuildCommandsFromBrowsers(
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
      BuildCommandsForBrowser(browser, tab_to_available_range,
                              windows_to_track);
    }
  }
}

void SessionServiceBase::ScheduleCommand(
    std::unique_ptr<sessions::SessionCommand> command) {
  DCHECK(command);
  if (ReplacePendingCommand(command_storage_manager_.get(), &command))
    return;

  bool is_closing_command = IsClosingCommand(command.get());
  command_storage_manager_->ScheduleCommand(std::move(command));
  // Don't schedule a reset on tab closed/window closed. Otherwise we may
  // lose tabs/windows we want to restore from if we exit right after this.
  if (!command_storage_manager_->pending_reset() &&
      command_storage_manager_->commands_since_reset() >= kWritesPerReset &&
      !is_closing_command) {
    ScheduleResetCommands();
  }
}

bool SessionServiceBase::ShouldTrackChangesToWindow(
    const SessionID& window_id) const {
  // This is overridden by session_service implementation.
  return true;
}

void SessionServiceBase::RebuildCommandsIfRequired() {
  if (rebuild_on_next_save_)
    ScheduleResetCommands();
}

sessions::CommandStorageManager*
SessionServiceBase::GetCommandStorageManagerForTest() {
  return command_storage_manager_.get();
}

void SessionServiceBase::SetAvailableRangeForTest(
    const SessionID& tab_id,
    const std::pair<int, int>& range) {
  tab_to_available_range_[tab_id] = range;
}

bool SessionServiceBase::GetAvailableRangeForTest(const SessionID& tab_id,
                                                  std::pair<int, int>* range) {
  auto i = tab_to_available_range_.find(tab_id);
  if (i == tab_to_available_range_.end())
    return false;

  *range = i->second;
  return true;
}
