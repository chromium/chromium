// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_manager.h"

#include <stddef.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_application_list_model.h"
#include "chrome/browser/background/background_mode_optimizer.h"
#include "chrome/browser/background/background_trigger.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/one_shot_event.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "chrome/browser/win/app_icon.h"
#endif

using base::UserMetricsAction;
using extensions::Extension;

namespace {

// Enum for recording menu item clicks in UMA.
// NOTE: Do not renumber these as that would confuse interpretation of
// previously logged data. When making changes, also update histograms.xml.
enum MenuItem {
  MENU_ITEM_ABOUT = 0,
  MENU_ITEM_TASK_MANAGER = 1,
  MENU_ITEM_BACKGROUND_CLIENT = 2,
  MENU_ITEM_KEEP_RUNNING = 3,
  MENU_ITEM_EXIT = 4,
  MENU_ITEM_NUM_STATES
};

void RecordMenuItemClick(MenuItem item) {
  UMA_HISTOGRAM_ENUMERATION("BackgroundMode.MenuItemClick", item,
                            MENU_ITEM_NUM_STATES);
}
}  // namespace

// static
bool BackgroundModeManager::should_restart_in_background_ = false;

BackgroundModeManager::BackgroundModeData::BackgroundModeData(
    Profile* profile,
    CommandIdHandlerVector* command_id_handler_vector)
    : applications_(std::make_unique<BackgroundApplicationListModel>(profile)),
      profile_(profile),
      command_id_handler_vector_(command_id_handler_vector) {}

BackgroundModeManager::BackgroundModeData::~BackgroundModeData() {
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager::BackgroundModeData, StatusIconMenuModel overrides
void BackgroundModeManager::BackgroundModeData::ExecuteCommand(
    int command_id,
    int event_flags) {
  switch (command_id) {
    case IDC_MinimumLabelValue:
      // Do nothing. This is just a label.
      break;
    default:
      DCHECK(!command_id_handler_vector_->at(command_id).is_null());
      RecordMenuItemClick(MENU_ITEM_BACKGROUND_CLIENT);
      command_id_handler_vector_->at(command_id).Run();
      break;
  }
}

Browser* BackgroundModeManager::BackgroundModeData::GetBrowserWindow() {
  return BackgroundModeManager::GetBrowserWindowForProfile(profile_);
}

bool BackgroundModeManager::BackgroundModeData::HasBackgroundClient() const {
  return applications_->size() > 0 || !registered_triggers_.empty();
}

void BackgroundModeManager::BackgroundModeData::BuildProfileMenu(
    StatusIconMenuModel* menu,
    StatusIconMenuModel* containing_menu) {
  if (HasBackgroundClient()) {
    // Add a menu item for each application (extension).
    for (const auto& application : *applications_) {
      const gfx::ImageSkia* icon = applications_->GetIcon(application.get());
      const std::string& name = application->name();
      int command_id = command_id_handler_vector_->size();
      // Check that the command ID is within the dynamic range.
      DCHECK_LT(command_id, IDC_MinimumLabelValue);
      command_id_handler_vector_->push_back(base::BindRepeating(
          &BackgroundModeManager::LaunchBackgroundApplication, profile_,
          base::RetainedRef(application)));
      menu->AddItem(command_id, base::UTF8ToUTF16(name));
      if (icon)
        menu->SetIcon(menu->GetItemCount() - 1, gfx::Image(*icon));

      // Component extensions with background that do not have an options page
      // will cause this menu item to go to the extensions page with an
      // absent component extension.
      //
      // Ideally, we would remove this item, but this conflicts with the user
      // model where this menu shows the extensions with background.
      //
      // The compromise is to disable the item, avoiding the non-actionable
      // navigate to the extensions page and preserving the user model.
      if (application->location() == extensions::Manifest::COMPONENT) {
        GURL options_page =
            extensions::OptionsPageInfo::GetOptionsPage(application.get());
        if (!options_page.is_valid())
          menu->SetCommandIdEnabled(command_id, false);
      }
    }

    // Add a menu item for each trigger.
    for (auto* trigger : registered_triggers_) {
      const gfx::ImageSkia* icon = trigger->GetIcon();
      const base::string16& name = trigger->GetName();
      int command_id = command_id_handler_vector_->size();
      // Check that the command ID is within the dynamic range.
      DCHECK_LT(command_id, IDC_MinimumLabelValue);
      command_id_handler_vector_->push_back(base::BindRepeating(
          &BackgroundTrigger::OnMenuClick, base::Unretained(trigger)));
      menu->AddItem(command_id, name);
      if (icon)
        menu->SetIcon(menu->GetItemCount() - 1, gfx::Image(*icon));
    }
  } else {
    // When there are no background clients, we want to display just a label
    // stating that none are running.
    menu->AddItemWithStringId(IDC_MinimumLabelValue,
                              IDS_BACKGROUND_APP_NOT_INSTALLED);
    menu->SetCommandIdEnabled(IDC_MinimumLabelValue, false);
  }
  if (containing_menu) {
    int menu_command_id = command_id_handler_vector_->size();
    // Check that the command ID is within the dynamic range.
    DCHECK_LT(menu_command_id, IDC_MinimumLabelValue);
    command_id_handler_vector_->push_back(base::DoNothing());
    containing_menu->AddSubMenu(menu_command_id, name_, menu);
  }
}

void BackgroundModeManager::BackgroundModeData::SetName(
    const base::string16& new_profile_name) {
  name_ = new_profile_name;
}

base::string16 BackgroundModeManager::BackgroundModeData::name() {
  return name_;
}

std::set<const extensions::Extension*>
BackgroundModeManager::BackgroundModeData::GetNewBackgroundApps() {
  std::set<const extensions::Extension*> new_apps;

  // Copy all current extensions into our list of |current_extensions_|.
  for (const auto& application : *applications_) {
    const extensions::ExtensionId& id = application->id();
    if (current_extensions_.count(id) == 0) {
      // Not found in our set yet - add it and maybe return as a previously
      // unseen extension.
      current_extensions_.insert(id);
      // If this application has been newly loaded after the initial startup,
      // notify the user.
      if (applications_->is_ready())
        new_apps.insert(application.get());
    }
  }
  return new_apps;
}

// static
bool BackgroundModeManager::BackgroundModeData::BackgroundModeDataCompare(
    const BackgroundModeData* bmd1,
    const BackgroundModeData* bmd2) {
  return bmd1->name_ < bmd2->name_;
}

void BackgroundModeManager::BackgroundModeData::AddPendingTrigger(
    BackgroundTrigger* trigger, bool should_notify_user) {
  if (HasTrigger(trigger))
    return;
  pending_trigger_data_[trigger] = should_notify_user;
}

BackgroundModeManager::PendingTriggerData
BackgroundModeManager::BackgroundModeData::TakePendingTriggerData() {
  return std::move(pending_trigger_data_);
}

bool BackgroundModeManager::BackgroundModeData::HasPendingTrigger() const {
  return !pending_trigger_data_.empty();
}

void BackgroundModeManager::BackgroundModeData::RegisterTrigger(
    BackgroundTrigger* trigger) {
  if (HasTrigger(trigger))
    return;
  registered_triggers_.insert(trigger);
}

void BackgroundModeManager::BackgroundModeData::UnregisterTrigger(
    BackgroundTrigger* trigger) {
  registered_triggers_.erase(trigger);
  pending_trigger_data_.erase(trigger);
}

bool BackgroundModeManager::BackgroundModeData::HasTrigger(
    BackgroundTrigger* trigger) {
  return base::ContainsKey(registered_triggers_, trigger) ||
         base::ContainsKey(pending_trigger_data_, trigger);
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, public
BackgroundModeManager::BackgroundModeManager(
    const base::CommandLine& command_line,
    ProfileAttributesStorage* profile_storage)
    : profile_storage_(profile_storage),
      task_runner_(CreateTaskRunner()),
      weak_factory_(this) {
  // We should never start up if there is no browser process or if we are
  // currently quitting.
  CHECK(g_browser_process);
  CHECK(!browser_shutdown::IsTryingToQuit());

  // Add self as an observer for the ProfileAttributesStorage so we know when
  // profiles are deleted and their names change.
  profile_storage_->AddObserver(this);

  UMA_HISTOGRAM_BOOLEAN("BackgroundMode.OnStartup.AutoLaunchState",
                        command_line.HasSwitch(switches::kNoStartupWindow));
  UMA_HISTOGRAM_BOOLEAN("BackgroundMode.OnStartup.IsBackgroundModePrefEnabled",
                        IsBackgroundModePrefEnabled());

  // Listen for the background mode preference changing.
  if (g_browser_process->local_state()) {  // Skip for unit tests
    pref_registrar_.Init(g_browser_process->local_state());
    pref_registrar_.Add(
        prefs::kBackgroundModeEnabled,
        base::BindRepeating(
            &BackgroundModeManager::OnBackgroundModeEnabledPrefChanged,
            base::Unretained(this)));
  }

  // Keep the browser alive until extensions are done loading - this is needed
  // by the --no-startup-window flag. We want to stay alive until we load
  // extensions, at which point we should either run in background mode (if
  // there are background apps) or exit if there are none.
  if (command_line.HasSwitch(switches::kNoStartupWindow)) {
    keep_alive_for_startup_.reset(
        new ScopedKeepAlive(KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,
                            KeepAliveRestartOption::DISABLED));
  } else {
    // Otherwise, start with background mode suspended in case we're launching
    // in a mode that doesn't open a browser window. It will be resumed when the
    // first browser window is opened.
    SuspendBackgroundMode();
    optimizer_ = BackgroundModeOptimizer::Create();
  }

  // If the -keep-alive-for-test flag is passed, then always keep chrome running
  // in the background until the user explicitly terminates it.
  if (command_line.HasSwitch(switches::kKeepAliveForTest))
    keep_alive_for_test_ = true;

  if (ShouldBeInBackgroundMode())
    StartBackgroundMode();

  // Listen for the application shutting down so we can release our KeepAlive.
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  BrowserList::AddObserver(this);
}

BackgroundModeManager::~BackgroundModeManager() {
  // Remove ourselves from the application observer list (only needed by unit
  // tests since APP_TERMINATING is what does this in a real running system).
  for (const auto& it : background_mode_data_)
    it.second->applications()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);

  // We're going away, so exit background mode (does nothing if we aren't in
  // background mode currently). This is primarily needed for unit tests,
  // because in an actual running system we'd get an APP_TERMINATING
  // notification before being destroyed.
  EndBackgroundMode();
}

// static
void BackgroundModeManager::RegisterPrefs(PrefRegistrySimple* registry) {
#if defined(OS_MACOSX)
  registry->RegisterBooleanPref(prefs::kUserRemovedLoginItem, false);
  registry->RegisterBooleanPref(prefs::kChromeCreatedLoginItem, false);
  registry->RegisterBooleanPref(prefs::kMigratedLoginItemPref, false);
#endif
  registry->RegisterBooleanPref(prefs::kBackgroundModeEnabled, true);
}

void BackgroundModeManager::RegisterProfile(Profile* profile) {
  // We don't want to register multiple times for one profile.
  DCHECK(!base::ContainsKey(background_mode_data_, profile));
  auto bmd = std::make_unique<BackgroundModeData>(profile,
                                                  &command_id_handler_vector_);
  BackgroundModeData* bmd_ptr = bmd.get();
  background_mode_data_[profile] = std::move(bmd);

  // Initially set the name for this background mode data.
  base::string16 name = l10n_util::GetStringUTF16(IDS_PROFILES_DEFAULT_NAME);
  ProfileAttributesEntry* entry;
  if (profile_storage_->GetProfileAttributesWithPath(
      profile->GetPath(), &entry)) {
    name = entry->GetName();
  }
  bmd_ptr->SetName(name);

  // Check for the presence of background apps after all extensions have been
  // loaded, to handle the case where an extension has been manually removed
  // while Chrome was not running.
  extensions::ExtensionSystem::Get(profile)->ready().Post(
      FROM_HERE,
      base::Bind(&BackgroundModeManager::OnExtensionsReady,
        weak_factory_.GetWeakPtr(), profile));

  bmd_ptr->applications()->AddObserver(this);

  // If we're adding a new profile and running in multi-profile mode, this new
  // profile should be added to the status icon if one currently exists.
  if (in_background_mode_ && status_icon_)
    UpdateStatusTrayIconContextMenu();
}

// static
void BackgroundModeManager::LaunchBackgroundApplication(
    Profile* profile,
    const Extension* extension) {
  OpenApplication(CreateAppLaunchParamsUserContainer(
      profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      extensions::SOURCE_BACKGROUND));
}

// static
Browser* BackgroundModeManager::GetBrowserWindowForProfile(Profile* profile) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  return browser ? browser : chrome::OpenEmptyWindow(profile);
}

bool BackgroundModeManager::IsBackgroundModeActive() {
  return in_background_mode_;
}

bool BackgroundModeManager::IsBackgroundWithoutWindows() const {
  return KeepAliveRegistry::GetInstance()->WouldRestartWithout({
      // Transient startup related KeepAlives, not related to any UI.
      KeepAliveOrigin::SESSION_RESTORE,
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,

      // Notification KeepAlives are not dependent on the Chrome UI being
      // loaded, and can be registered when we were in pure background mode.
      // They just block it to avoid issues. Ignore them when determining if we
      // are in that mode.
      KeepAliveOrigin::NOTIFICATION,
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
      KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE,
  });
}

size_t BackgroundModeManager::NumberOfBackgroundModeData() {
  return background_mode_data_.size();
}

void BackgroundModeManager::RegisterTrigger(const Profile* profile,
                                            BackgroundTrigger* trigger,
                                            bool should_notify_user) {
  BackgroundModeManager::BackgroundModeData* bmd =
      GetBackgroundModeData(profile);
  if (!bmd)
    return;

  // Only proceed if we don't have this trigger yet.
  if (bmd->HasTrigger(trigger))
    return;

  // If the background pref is disabled, store it as a pending trigger that may
  // be registered later if the pref gets enabled.
  if (!IsBackgroundModePrefEnabled()) {
    bmd->AddPendingTrigger(trigger, should_notify_user);
    return;
  }

  bmd->RegisterTrigger(trigger);
  std::vector<base::string16> new_client_names;
  if (should_notify_user)
    new_client_names.push_back(trigger->GetName());
  OnClientsChanged(profile, new_client_names);
}

void BackgroundModeManager::UnregisterTrigger(const Profile* profile,
                                              BackgroundTrigger* trigger) {
  if (!IsBackgroundModePrefEnabled())
    return;

  BackgroundModeManager::BackgroundModeData* bmd =
      GetBackgroundModeData(profile);
  if (!bmd)
    return;

  // Only proceed if this is a known trigger.
  if (!bmd->HasTrigger(trigger))
    return;
  bmd->UnregisterTrigger(trigger);

  std::vector<base::string16> new_client_names;
  OnClientsChanged(profile, new_client_names);
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, content::NotificationObserver overrides
void BackgroundModeManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);

  // Make sure we aren't still keeping the app alive (only happens if we
  // don't receive an EXTENSIONS_READY notification for some reason).
  ReleaseStartupKeepAlive();
  // Performing an explicit shutdown, so exit background mode (does nothing
  // if we aren't in background mode currently).
  EndBackgroundMode();
  // Shutting down, so don't listen for any more notifications so we don't
  // try to re-enter/exit background mode again.
  registrar_.RemoveAll();
  for (const auto& it : background_mode_data_)
    it.second->applications()->RemoveObserver(this);
}

void BackgroundModeManager::OnExtensionsReady(const Profile* profile) {
  BackgroundModeManager::BackgroundModeData* bmd =
      GetBackgroundModeData(profile);
  if (bmd) {
    UMA_HISTOGRAM_COUNTS_100("BackgroundMode.BackgroundApplicationsCount",
                             bmd->applications()->size());
  }
  // Extensions are loaded, so we don't need to manually keep the browser
  // process alive any more when running in no-startup-window mode.
  ReleaseStartupKeepAlive();
}

void BackgroundModeManager::OnBackgroundModeEnabledPrefChanged() {
  bool enabled = IsBackgroundModePrefEnabled();
  UMA_HISTOGRAM_BOOLEAN("BackgroundMode.BackgroundModeEnabledPrefChanged",
                        enabled);
  if (enabled)
    EnableBackgroundMode();
  else
    DisableBackgroundMode();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, BackgroundApplicationListModel::Observer overrides
void BackgroundModeManager::OnApplicationDataChanged() {
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::OnApplicationListChanged(const Profile* profile) {
  if (!IsBackgroundModePrefEnabled())
    return;

  BackgroundModeManager::BackgroundModeData* bmd =
      GetBackgroundModeData(profile);
  if (!bmd)
    return;

  // Get the new apps (if any) and process them.
  std::set<const extensions::Extension*> new_apps = bmd->GetNewBackgroundApps();
  std::vector<base::string16> new_names;
  for (auto* app : new_apps)
    new_names.push_back(base::UTF8ToUTF16(app->name()));
  OnClientsChanged(profile, new_names);
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, ProfileAttributesStorage::Observer overrides
void BackgroundModeManager::OnProfileAdded(const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;
  bool success =
      profile_storage_->GetProfileAttributesWithPath(profile_path, &entry);
  DCHECK(success);
  base::string16 profile_name = entry->GetName();
  // At this point, the profile should be registered with the background mode
  // manager, but when it's actually added to the ProfileAttributesStorage is
  // when its name is set so we need up to update that with the
  // background_mode_data.
  for (const auto& it : background_mode_data_) {
    if (it.first->GetPath() == profile_path) {
      it.second->SetName(profile_name);
      UpdateStatusTrayIconContextMenu();
      return;
    }
  }
}

void BackgroundModeManager::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;
  bool success =
      profile_storage_->GetProfileAttributesWithPath(profile_path, &entry);
  DCHECK(success);
  base::string16 profile_name = entry->GetName();
  // Remove the profile from our map of profiles.
  auto it = GetBackgroundModeIterator(profile_name);
  // If a profile isn't running a background app, it may not be in the map.
  if (it != background_mode_data_.end()) {
    it->second->applications()->RemoveObserver(this);
    background_mode_data_.erase(it);
    // If there are no background mode profiles any longer, then turn off
    // background mode.
    if (!ShouldBeInBackgroundMode()) {
      EnableLaunchOnStartup(false);
      EndBackgroundMode();
    }
    UpdateStatusTrayIconContextMenu();
  }
}

void BackgroundModeManager::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  ProfileAttributesEntry* entry;
  bool success =
      profile_storage_->GetProfileAttributesWithPath(profile_path, &entry);
  DCHECK(success);
  base::string16 new_profile_name = entry->GetName();
  BackgroundModeInfoMap::const_iterator it =
      GetBackgroundModeIterator(old_profile_name);
  // We check that the returned iterator is valid due to unittests, but really
  // this should only be called on profiles already known by the background
  // mode manager.
  if (it != background_mode_data_.end()) {
    it->second->SetName(new_profile_name);
    UpdateStatusTrayIconContextMenu();
  }
}

BackgroundModeManager::BackgroundModeData*
BackgroundModeManager::GetBackgroundModeDataForLastProfile() const {
  Profile* most_recent_profile = g_browser_process->profile_manager()->
      GetLastUsedProfileAllowedByPolicy();
  auto profile_background_data =
      background_mode_data_.find(most_recent_profile);

  if (profile_background_data == background_mode_data_.end())
    return nullptr;

  // Do not permit a locked profile to be used to open a browser.
  ProfileAttributesEntry* entry;
  bool success = profile_storage_->GetProfileAttributesWithPath(
      profile_background_data->first->GetPath(), &entry);
  DCHECK(success);
  if (entry->IsSigninRequired())
    return nullptr;

  return profile_background_data->second.get();
}

///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager::BackgroundModeData, StatusIconMenuModel overrides
void BackgroundModeManager::ExecuteCommand(int command_id, int event_flags) {
  BackgroundModeData* bmd = GetBackgroundModeDataForLastProfile();
  switch (command_id) {
    case IDC_ABOUT:
      RecordMenuItemClick(MENU_ITEM_ABOUT);
      if (bmd) {
        chrome::ShowAboutChrome(bmd->GetBrowserWindow());
      } else {
        UserManager::Show(base::FilePath(),
                          profiles::USER_MANAGER_SELECT_PROFILE_ABOUT_CHROME);
      }
      break;
    case IDC_TASK_MANAGER:
    RecordMenuItemClick(MENU_ITEM_TASK_MANAGER);
      if (bmd) {
        chrome::OpenTaskManager(bmd->GetBrowserWindow());
      } else {
        UserManager::Show(base::FilePath(),
                          profiles::USER_MANAGER_SELECT_PROFILE_TASK_MANAGER);
      }
      break;
    case IDC_EXIT:
      RecordMenuItemClick(MENU_ITEM_EXIT);
      base::RecordAction(UserMetricsAction("Exit"));
      chrome::CloseAllBrowsers();
      break;
    case IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND: {
      // Background mode must already be enabled (as otherwise this menu would
      // not be visible).
      DCHECK(IsBackgroundModePrefEnabled());
      DCHECK(KeepAliveRegistry::GetInstance()->IsKeepingAlive());

      RecordMenuItemClick(MENU_ITEM_KEEP_RUNNING);

      // Set the background mode pref to "disabled" - the resulting notification
      // will result in a call to DisableBackgroundMode().
      PrefService* service = g_browser_process->local_state();
      DCHECK(service);
      service->SetBoolean(prefs::kBackgroundModeEnabled, false);
      break;
    }
    default:
      if (bmd) {
        bmd->ExecuteCommand(command_id, event_flags);
      } else {
        UserManager::Show(base::FilePath(),
                          profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
      }
      break;
  }
}


///////////////////////////////////////////////////////////////////////////////
//  BackgroundModeManager, private
void BackgroundModeManager::ReleaseStartupKeepAliveCallback() {
  keep_alive_for_startup_.reset();
  optimizer_ = BackgroundModeOptimizer::Create();
}

void BackgroundModeManager::ReleaseStartupKeepAlive() {
  if (keep_alive_for_startup_) {
    // We call this via the message queue to make sure we don't try to end
    // keep-alive (which can shutdown Chrome) before the message loop has
    // started. This object reference is safe because it's going to be kept
    // alive by the browser process until after the callback is called.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BackgroundModeManager::ReleaseStartupKeepAliveCallback,
                       base::Unretained(this)));
  }
}

void BackgroundModeManager::StartBackgroundMode() {
  DCHECK(ShouldBeInBackgroundMode());
  // Don't bother putting ourselves in background mode if we're already there
  // or if background mode is disabled.
  if (in_background_mode_)
    return;

  startup_metric_utils::SetBackgroundModeEnabled();

  // Mark ourselves as running in background mode.
  in_background_mode_ = true;

  UpdateKeepAliveAndTrayIcon();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_MODE_CHANGED,
      content::Source<BackgroundModeManager>(this),
      content::Details<bool>(&in_background_mode_));
}

void BackgroundModeManager::EndBackgroundMode() {
  if (!in_background_mode_)
    return;
  in_background_mode_ = false;

  UpdateKeepAliveAndTrayIcon();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BACKGROUND_MODE_CHANGED,
      content::Source<BackgroundModeManager>(this),
      content::Details<bool>(&in_background_mode_));
}

void BackgroundModeManager::EnableBackgroundMode() {
  DCHECK(IsBackgroundModePrefEnabled());
  // If background mode should be enabled, but isn't, turn it on.
  if (!in_background_mode_ && ShouldBeInBackgroundMode()) {
    StartBackgroundMode();

    // Register pending triggers.
    for (const auto& bmd_iterator : background_mode_data_) {
      PendingTriggerData pending_trigger_data =
          bmd_iterator.second->TakePendingTriggerData();
      for (const auto& trigger_data_iterator : pending_trigger_data) {
        const Profile* profile = bmd_iterator.first;
        BackgroundTrigger* trigger = trigger_data_iterator.first;
        bool should_notify_user = trigger_data_iterator.second;
        RegisterTrigger(profile, trigger, should_notify_user);
      }
    }

    EnableLaunchOnStartup(true);
  }
}

void BackgroundModeManager::DisableBackgroundMode() {
  DCHECK(!IsBackgroundModePrefEnabled());
  // If background mode is currently enabled, turn it off.
  if (in_background_mode_) {
    EndBackgroundMode();
    EnableLaunchOnStartup(false);
  }
}

void BackgroundModeManager::SuspendBackgroundMode() {
  background_mode_suspended_ = true;
  UpdateKeepAliveAndTrayIcon();
}

void BackgroundModeManager::ResumeBackgroundMode() {
  background_mode_suspended_ = false;
  UpdateKeepAliveAndTrayIcon();
}

void BackgroundModeManager::UpdateKeepAliveAndTrayIcon() {
  if (in_background_mode_ && !background_mode_suspended_) {
    if (!keep_alive_) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::BACKGROUND_MODE_MANAGER,
          KeepAliveRestartOption::ENABLED);
    }
    CreateStatusTrayIcon();
    return;
  }

  RemoveStatusTrayIcon();
  keep_alive_.reset();
}

void BackgroundModeManager::OnBrowserAdded(Browser* browser) {
  ResumeBackgroundMode();
}

void BackgroundModeManager::OnClientsChanged(
    const Profile* profile,
    const std::vector<base::string16>& new_client_names) {
  DCHECK(IsBackgroundModePrefEnabled());

  // Update the ProfileAttributesStorage with the fact whether background
  // clients are running for this profile.
  ProfileAttributesEntry* entry;
  if (profile_storage_->
      GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
    entry->SetBackgroundStatus(HasBackgroundClientForProfile(profile));
  }

  if (!ShouldBeInBackgroundMode()) {
    // We've uninstalled our last background client, make sure we exit
    // background mode and no longer launch on startup.
    EnableLaunchOnStartup(false);
    EndBackgroundMode();
  } else {
    // We have at least one background client - make sure we're in background
    // mode.
    if (!in_background_mode_) {
      // We're entering background mode - make sure we have launch-on-startup
      // enabled. On Mac, the platform-specific code tracks whether the user
      // has deleted a login item in the past, and if so, no login item will
      // be created (to avoid overriding the specific user action).
      EnableLaunchOnStartup(true);
      StartBackgroundMode();
    }

    // List of clients changed so update the UI.
    UpdateStatusTrayIconContextMenu();

    // Notify the user about any new clients.
    for (const auto& name : new_client_names)
      OnBackgroundClientInstalled(name);
  }
}

bool BackgroundModeManager::HasBackgroundClient() const {
  for (const auto& it : background_mode_data_) {
    if (it.second->HasBackgroundClient())
      return true;
  }
  return false;
}

bool BackgroundModeManager::HasBackgroundClientForProfile(
    const Profile* profile) const {
  BackgroundModeManager::BackgroundModeData* bmd =
      GetBackgroundModeData(profile);
  return bmd && bmd->HasBackgroundClient();
}

bool BackgroundModeManager::HasPendingTrigger() const {
  for (const auto& it : background_mode_data_) {
    if (it.second->HasPendingTrigger())
      return true;
  }
  return false;
}

bool BackgroundModeManager::ShouldBeInBackgroundMode() const {
  return IsBackgroundModePrefEnabled() &&
         (HasBackgroundClient() || HasPendingTrigger() || keep_alive_for_test_);
}

void BackgroundModeManager::OnBackgroundClientInstalled(
    const base::string16& name) {
  // Background mode is disabled - don't do anything.
  if (!IsBackgroundModePrefEnabled())
    return;

  // Ensure we have a tray icon (needed so we can display the app-installed
  // notification below).
  EnableBackgroundMode();
  ResumeBackgroundMode();

  // Notify the user that a background client has been installed.
  DisplayClientInstalledNotification(name);
}

// Gets the image for the status tray icon, at the correct size for the current
// platform and display settings.
gfx::ImageSkia GetStatusTrayIcon() {
#if defined(OS_WIN)
  // On Windows, use GetSmallAppIconSize to get the correct image size. The
  // user's "text size" setting in Windows determines how large the system tray
  // icon should be.
  gfx::Size size = GetSmallAppIconSize();

  // This loads all of the icon images, which is a bit wasteful because we're
  // going to pick one and throw the rest away, but that is the price of using
  // the ImageFamily abstraction. Note: We could just use the LoadImage function
  // from the Windows API, but that does a *terrible* job scaling images.
  // Therefore, we fetch the images and do our own high-quality scaling.
  std::unique_ptr<gfx::ImageFamily> family = GetAppIconImageFamily();
  DCHECK(family);
  if (!family)
    return gfx::ImageSkia();

  return family->CreateExact(size).AsImageSkia();
#else
  // On other platforms, just get a static resource image.
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_STATUS_TRAY_ICON);
#endif
}

void BackgroundModeManager::CreateStatusTrayIcon() {
  // Only need status icons on windows/linux. ChromeOS doesn't allow exiting
  // Chrome and Mac can use the dock icon instead.

  // Since there are multiple profiles which share the status tray, we now
  // use the browser process to keep track of it.
#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
  if (!status_tray_)
    status_tray_ = g_browser_process->status_tray();
#endif

  // If the platform doesn't support status icons, or we've already created
  // our status icon, just return.
  if (!status_tray_ || status_icon_)
    return;

  status_icon_ = status_tray_->CreateStatusIcon(
      StatusTray::BACKGROUND_MODE_ICON, GetStatusTrayIcon(),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  if (!status_icon_)
    return;
  UpdateStatusTrayIconContextMenu();
}

void BackgroundModeManager::UpdateStatusTrayIconContextMenu() {
  // Ensure we have a tray icon if appropriate.
  UpdateKeepAliveAndTrayIcon();

  // If we don't have a status icon or one could not be created succesfully,
  // then no need to continue the update.
  if (!status_icon_)
    return;

  // We should only get here if we have a profile loaded, or if we're running
  // in test mode.
  if (background_mode_data_.empty()) {
    DCHECK(keep_alive_for_test_);
    return;
  }

  command_id_handler_vector_.clear();
  submenus.clear();

  std::unique_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(this));
  menu->AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
  menu->AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  menu->AddSeparator(ui::NORMAL_SEPARATOR);

  // If there are multiple profiles they each get a submenu.
  if (profile_storage_->GetNumberOfProfiles() > 1) {
    std::vector<BackgroundModeData*> bmd_vector;
    for (const auto& it : background_mode_data_)
      bmd_vector.push_back(it.second.get());
    std::sort(bmd_vector.begin(), bmd_vector.end(),
              &BackgroundModeData::BackgroundModeDataCompare);
    int profiles_using_background_mode = 0;
    for (auto* bmd : bmd_vector) {
      // We should only display the profile in the status icon if it has at
      // least one background app.
      if (bmd->HasBackgroundClient()) {
        // The submenu constructor caller owns the lifetime of the submenu.
        // The containing menu does not handle the lifetime.
        submenus.push_back(std::make_unique<StatusIconMenuModel>(bmd));
        bmd->BuildProfileMenu(submenus.back().get(), menu.get());
        profiles_using_background_mode++;
      }
    }
    // We should only be displaying the status tray icon if there is at least
    // one profile using background mode. If |keep_alive_for_test_| is set,
    // there may not be any profiles and that is okay.
    DCHECK(profiles_using_background_mode > 0 || keep_alive_for_test_);
  } else {
    // We should only have one profile in the ProfileAttributesStorage if we are
    // not using multi-profiles. If |keep_alive_for_test_| is set, then we may
    // not have any profiles in the ProfileAttributesStorage.
    DCHECK(profile_storage_->GetNumberOfProfiles() == size_t(1) ||
           keep_alive_for_test_);
    background_mode_data_.begin()->second->BuildProfileMenu(menu.get(),
                                                            nullptr);
  }

  menu->AddSeparator(ui::NORMAL_SEPARATOR);
  menu->AddCheckItemWithStringId(
      IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND,
      IDS_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND);
  menu->SetCommandIdChecked(IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND,
                            true);

  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  bool enabled =
      service->IsUserModifiablePreference(prefs::kBackgroundModeEnabled);
  menu->SetCommandIdEnabled(IDC_STATUS_TRAY_KEEP_CHROME_RUNNING_IN_BACKGROUND,
                            enabled);

  menu->AddItemWithStringId(IDC_EXIT, IDS_EXIT);

  context_menu_ = menu.get();
  status_icon_->SetContextMenu(std::move(menu));
}

void BackgroundModeManager::RemoveStatusTrayIcon() {
  if (status_icon_)
    status_tray_->RemoveStatusIcon(status_icon_);
  status_icon_ = nullptr;
  context_menu_ = nullptr;
}

BackgroundModeManager::BackgroundModeData*
BackgroundModeManager::GetBackgroundModeData(const Profile* profile) const {
  // Profiles are shut down and destroyed asynchronously after
  // OnProfileWillBeRemoved is called, so we may have dropped anything
  // associated with the profile already.
  auto it = background_mode_data_.find(profile);
  return it != background_mode_data_.end() ? it->second.get() : nullptr;
}

BackgroundModeManager::BackgroundModeInfoMap::iterator
BackgroundModeManager::GetBackgroundModeIterator(
    const base::string16& profile_name) {
  auto profile_it = background_mode_data_.end();
  for (auto it = background_mode_data_.begin();
       it != background_mode_data_.end(); ++it) {
    if (it->second->name() == profile_name) {
      profile_it = it;
    }
  }
  return profile_it;
}

bool BackgroundModeManager::IsBackgroundModePrefEnabled() const {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBackgroundModeEnabled);
}
