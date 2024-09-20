// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_base.h"

#include <stddef.h>

#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "ui/base/mojom/window_show_state.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::Time;
using content::NavigationEntry;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace {

// Every kWritesPerReset commands triggers recreating the file.
const int kWritesPerReset = 250;

// User data key for WebContents to derive their types.
const void* const kSessionServiceBaseUserDataKey =
    &kSessionServiceBaseUserDataKey;

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

// SessionServiceBaseUserData
// -------------------------------------------------------------
class SessionServiceBaseUserData : public base::SupportsUserData::Data {
 public:
  explicit SessionServiceBaseUserData(Browser::Type type) : type_(type) {}
  ~SessionServiceBaseUserData() override = default;
  SessionServiceBaseUserData(const SessionServiceBaseUserData&) = delete;
  SessionServiceBaseUserData& operator=(const SessionServiceBaseUserData&) =
      delete;

  Browser::Type type() const { return type_; }

 private:
  const Browser::Type type_;
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

// static
Browser::Type SessionServiceBase::GetBrowserTypeFromWebContents(
    content::WebContents* web_contents) {
  SessionServiceBaseUserData* data = static_cast<SessionServiceBaseUserData*>(
      web_contents->GetUserData(&kSessionServiceBaseUserDataKey));

  // Browser tab WebContents will have the UserData set on them. However, it is
  // possible that WebContents that are not tabs call into this code.
  // In that case, data will be null and we just return TYPE_NORMAL.
  if (!data)
    return Browser::Type::TYPE_NORMAL;

  return data->type();
}

void SessionServiceBase::SetWindowVisibleOnAllWorkspaces(
    SessionID window_id,
    bool visible_on_all_workspaces) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetWindowVisibleOnAllWorkspacesCommand(
      window_id, visible_on_all_workspaces));
}

void SessionServiceBase::ResetFromCurrentBrowsers() {
  if (is_saving_enabled_)
    ScheduleResetCommands();
}

void SessionServiceBase::SetTabWindow(SessionID window_id, SessionID tab_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetTabWindowCommand(window_id, tab_id));
}

void SessionServiceBase::SetWindowBounds(
    SessionID window_id,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowBoundsCommand(window_id, bounds, show_state));
}

void SessionServiceBase::SetWindowWorkspace(SessionID window_id,
                                            const std::string& workspace) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowWorkspaceCommand(window_id, workspace));
}

void SessionServiceBase::SetTabIndexInWindow(SessionID window_id,
                                             SessionID tab_id,
                                             int new_index) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabIndexInWindowCommand(tab_id, new_index));
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

  // The tab being inserted is most likely already registered by
  // Browser::WebContentsCreated, but just register the UserData again.
  contents->SetUserData(&kSessionServiceBaseUserDataKey,
                        std::make_unique<SessionServiceBaseUserData>(
                            GetDesiredBrowserTypeForWebContents()));
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

void SessionServiceBase::TabRestored(WebContents* tab, bool pinned) {
  if (!is_saving_enabled_)
    return;

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab);
  if (!ShouldTrackChangesToWindow(session_tab_helper->window_id()))
    return;

  BuildCommandsForTab(session_tab_helper->window_id(), tab, -1, std::nullopt,
                      pinned, nullptr);
  command_storage_manager()->StartSaveTimer();
}

void SessionServiceBase::SetSelectedTabInWindow(SessionID window_id,
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
    SessionID window_id,
    SessionID tab_id,
    const std::string& extension_app_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetTabExtensionAppIDCommand(tab_id, extension_app_id));
}

void SessionServiceBase::SetLastActiveTime(SessionID window_id,
                                           SessionID tab_id,
                                           base::Time last_active_time) {
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

void SessionServiceBase::SetWindowAppName(SessionID window_id,
                                          const std::string& app_name) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetWindowAppNameCommand(window_id, app_name));
}

void SessionServiceBase::SetPinnedState(SessionID window_id,
                                        SessionID tab_id,
                                        bool is_pinned) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreatePinnedStateCommand(tab_id, is_pinned));
}

bool SessionServiceBase::ShouldUseDelayedSave() {
  return should_use_delayed_save_;
}

void SessionServiceBase::OnWillSaveCommands() {
  if (!is_saving_enabled_) {
    // There should be no commands scheduled, otherwise data will be written,
    // potentially clobbering the last file.
    DCHECK(command_storage_manager_->pending_commands().empty());
    return;
  }

  RebuildCommandsIfRequired();
  did_save_commands_at_least_once_ |=
      !command_storage_manager()->pending_commands().empty();
}

void SessionServiceBase::OnErrorWritingSessionCommands() {
  LogSessionServiceWriteErrorEvent(profile_, false);
  rebuild_on_next_save_ = true;
  RebuildCommandsIfRequired();
}

void SessionServiceBase::SetTabUserAgentOverride(
    SessionID window_id,
    SessionID tab_id,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  // This is overridden by session_service implementation.
  // We still need it here because we derive from
  // sessions::SessionTabHelperDelegate.
  NOTREACHED_IN_MIGRATION();
  return;
}

void SessionServiceBase::SetSelectedNavigationIndex(SessionID window_id,
                                                    SessionID tab_id,
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
    SessionID window_id,
    SessionID tab_id,
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

void SessionServiceBase::TabNavigationPathPruned(SessionID window_id,
                                                 SessionID tab_id,
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

void SessionServiceBase::TabNavigationPathEntriesDeleted(SessionID window_id,
                                                         SessionID tab_id) {
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
  std::vector<std::unique_ptr<sessions::SessionWindow>> valid_windows;
  SessionID active_window_id = SessionID::InvalidValue();

  sessions::RestoreSessionFromCommands(commands, &valid_windows,
                                       &active_window_id);
  RemoveUnusedRestoreWindows(&valid_windows);

  std::move(callback).Run(std::move(valid_windows), active_window_id,
                          read_error);
}

void SessionServiceBase::BuildCommandsForTab(
    SessionID window_id,
    WebContents* tab,
    int index_in_window,
    std::optional<tab_groups::TabGroupId> group,
    bool is_pinned,
    IdToRange* tab_to_available_range) {
  DCHECK(tab);
  DCHECK(window_id.is_valid());

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab);
  const SessionID session_id(session_tab_helper->session_id());
  command_storage_manager()->AppendRebuildCommand(
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

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateLastActiveTimeCommand(session_id,
                                            tab->GetLastActiveTime()));

  // TODO(stahon@microsoft.com) This might be movable to SessionService
  // when Chrome OS uses AppSessionService for app restores.
  // For now it needs to stay to support SessionService restoring apps.
  std::string app_id = apps::GetAppIdForWebContents(tab);
  if (!app_id.empty()) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateSetTabExtensionAppIDCommand(session_id, app_id));
  }

  for (int i = min_index; i < max_index; ++i) {
    NavigationEntry* entry = (i == pending_index)
                                 ? tab->GetController().GetPendingEntry()
                                 : tab->GetController().GetEntryAtIndex(i);
    DCHECK(entry);
    if (ShouldTrackURLForRestore(entry->GetVirtualURL()) &&
        !entry->IsInitialEntry()) {
      // Don't try to persist initial NavigationEntry, as it is not actually
      // associated with any navigation and will just result in about:blank on
      // session restore.
      const SerializedNavigationEntry navigation =
          ContentSerializedNavigationBuilder::FromNavigationEntry(i, entry);
      command_storage_manager()->AppendRebuildCommand(
          CreateUpdateTabNavigationCommand(session_id, navigation));
    }
  }
  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetSelectedNavigationIndexCommand(session_id,
                                                        current_index));

  if (index_in_window != -1) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateSetTabIndexInWindowCommand(session_id,
                                                   index_in_window));
  }

  if (is_pinned) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreatePinnedStateCommand(session_id, true));
  }

  // Record the association between the sessionStorage namespace and the tab.
  content::SessionStorageNamespace* session_storage_namespace =
      tab->GetController().GetDefaultSessionStorageNamespace();
  ScheduleCommand(sessions::CreateSessionStorageAssociatedCommand(
      session_tab_helper->session_id(), session_storage_namespace->id()));
}

void SessionServiceBase::BuildCommandsForBrowser(
    Browser* browser,
    IdToRange* tab_to_available_range,
    std::set<SessionID>* windows_to_track) {
  DCHECK(is_saving_enabled_);
  DCHECK(browser);
  DCHECK(browser->session_id().is_valid());

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetWindowBoundsCommand(
          browser->session_id(), browser->window()->GetRestoredBounds(),
          browser->window()->GetRestoredState()));

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetWindowTypeCommand(
          browser->session_id(), WindowTypeForBrowserType(browser->type())));

  if (!browser->app_name().empty()) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateSetWindowAppNameCommand(browser->session_id(),
                                                browser->app_name()));
  }

  if (!browser->user_title().empty()) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateSetWindowUserTitleCommand(browser->session_id(),
                                                  browser->user_title()));
  }

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetWindowWorkspaceCommand(
          browser->session_id(), browser->window()->GetWorkspace()));

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetWindowVisibleOnAllWorkspacesCommand(
          browser->session_id(),
          browser->window()->IsVisibleOnAllWorkspaces()));

  command_storage_manager()->AppendRebuildCommand(
      sessions::CreateSetSelectedTabInWindowCommand(
          browser->session_id(), browser->tab_strip_model()->active_index()));

  // Set the visual data for each tab group.
  TabStripModel* tab_strip = browser->tab_strip_model();
  if (tab_strip->SupportsTabGroups()) {
    TabGroupModel* group_model = tab_strip->group_model();
    tab_groups::TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser->profile());

    for (const tab_groups::TabGroupId& group_id :
         group_model->ListTabGroups()) {
      const tab_groups::TabGroupVisualData* visual_data =
          group_model->GetTabGroup(group_id)->visual_data();

      std::optional<std::string> saved_guid;
      if (tab_group_service) {
        const std::optional<tab_groups::SavedTabGroup> saved_group =
            tab_group_service->GetGroup(group_id);
        if (saved_group.has_value()) {
          saved_guid = saved_group->saved_guid().AsLowercaseString();
        }
      }

      command_storage_manager()->AppendRebuildCommand(
          sessions::CreateTabGroupMetadataUpdateCommand(group_id, visual_data,
                                                        std::move(saved_guid)));
    }
  }

  for (int i = 0; i < tab_strip->count(); ++i) {
    WebContents* tab = tab_strip->GetWebContentsAt(i);
    DCHECK(tab);
    const std::optional<tab_groups::TabGroupId> group_id =
        tab_strip->GetTabGroupForTab(i);
    BuildCommandsForTab(browser->session_id(), tab, i, group_id,
                        tab_strip->IsTabPinned(i), tab_to_available_range);
  }

  windows_to_track->insert(browser->session_id());
}

void SessionServiceBase::BuildCommandsFromBrowsers(
    IdToRange* tab_to_available_range,
    std::set<SessionID>* windows_to_track) {
  DCHECK(is_saving_enabled_);
  for (Browser* browser : *BrowserList::GetInstance()) {
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
  if (!is_saving_enabled_)
    return;

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
  DidScheduleCommand();
}

bool SessionServiceBase::ShouldTrackChangesToWindow(SessionID window_id) const {
  return windows_tracking_.find(window_id) != windows_tracking_.end();
}

bool SessionServiceBase::ShouldTrackBrowser(Browser* browser) const {
  if (browser->profile() != profile())
    return false;

  if (browser->omit_from_session_restore())
    return false;

  // Never track app popup windows that do not have a trusted source (i.e.
  // popup windows spawned by an app). If this logic changes, be sure to also
  // change SessionRestoreImpl::CreateRestoredBrowser().
  if ((browser->is_type_app() || browser->is_type_app_popup()) &&
      !browser->is_trusted_source()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Windows that are auto-started and prevented from closing are exempted from
  // tracking for session restore to prevent multiple unclosable open instances
  // of the same app.
  web_app::AppBrowserController* app_controller = browser->app_controller();
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile());
  // Checking for close prevention does not require an `AppLock` and
  // therefore `registrar_unsafe()` is safe to use.
  if (app_controller && provider &&
      provider->registrar_unsafe().IsPreventCloseEnabled(
          app_controller->app_id())) {
    return false;
  }
#endif  // #if BUILDFLAG(IS_CHROMEOS)

  return ShouldRestoreWindowOfType(WindowTypeForBrowserType(browser->type()));
}

sessions::CommandStorageManager*
SessionServiceBase::GetCommandStorageManagerForTest() {
  return command_storage_manager_.get();
}

void SessionServiceBase::SetAvailableRangeForTest(
    SessionID tab_id,
    const std::pair<int, int>& range) {
  tab_to_available_range_[tab_id] = range;
}

bool SessionServiceBase::GetAvailableRangeForTest(SessionID tab_id,
                                                  std::pair<int, int>* range) {
  auto i = tab_to_available_range_.find(tab_id);
  if (i == tab_to_available_range_.end())
    return false;

  *range = i->second;
  return true;
}

void SessionServiceBase::SetSavingEnabled(bool enabled) {
  if (is_saving_enabled_ == enabled)
    return;
  is_saving_enabled_ = enabled;
  if (!is_saving_enabled_) {
    // Transitioning from enabled to disabled should happen very early on,
    // before any commands are actually written. If commands are written, then
    // the purpose of disabling will have failed (because by writing some
    // commands the previous session is going to be lost on exit).
    DCHECK(!did_save_commands_at_least_once_);
    command_storage_manager()->ClearPendingCommands();
  } else {
    ScheduleResetCommands();
  }
}
