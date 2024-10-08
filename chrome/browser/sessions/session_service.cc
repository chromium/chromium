// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/session_crashed_bubble.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/browser_launcher.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_utils.h"
#endif  // defined(TOOLKIT_VIEWS)

using content::NavigationEntry;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace {

const void* const kInstanceTrackerKey = &kInstanceTrackerKey;

// An instance of this class is created pre-profile. It is used to track how
// many SessionServices have been created.
class InstanceTracker : public base::SupportsUserData::Data {
 public:
  InstanceTracker() = default;
  InstanceTracker(const InstanceTracker&) = delete;
  InstanceTracker& operator=(const InstanceTracker&) = delete;
  ~InstanceTracker() override = default;

  // Registers a new instance. Returns true is this is the first time called
  // with the specified profile.
  static bool RegisterNewSessionService(Profile* profile) {
    InstanceTracker* tracker = static_cast<InstanceTracker*>(
        profile->GetUserData(kInstanceTrackerKey));
    if (!tracker) {
      profile->SetUserData(kInstanceTrackerKey,
                           std::make_unique<InstanceTracker>());
      tracker = static_cast<InstanceTracker*>(
          profile->GetUserData(kInstanceTrackerKey));
    }
    return tracker->IncrementSessionServiceCount();
  }

 private:
  bool IncrementSessionServiceCount() {
    const bool is_first = session_service_count_ == 0;
    ++session_service_count_;
    return is_first;
  }

  int session_service_count_ = 0;
};

}  // namespace

SessionService::SessionService(Profile* profile)
    : SessionServiceBase(
          profile,
          SessionServiceBase::SessionServiceType::kSessionRestore),
      is_first_session_service_(
          InstanceTracker::RegisterNewSessionService(profile)) {
  if (is_first_session_service_)
    LogSessionServiceStartEvent(profile, HasPendingUncleanExit(profile));
  closing_all_browsers_subscription_ = chrome::AddClosingAllBrowsersCallback(
      base::BindRepeating(&SessionService::OnClosingAllBrowsersChanged,
                          base::Unretained(this)));
  ExitTypeService* exit_type_service =
      ExitTypeService::GetInstanceForProfile(profile);
  if (exit_type_service && exit_type_service->waiting_for_user_to_ack_crash()) {
    SetSavingEnabled(false);
    exit_type_service->AddCrashAckCallback(base::BindOnce(
        &SessionService::SetSavingEnabled, weak_factory_.GetWeakPtr(), true));
  }
}

SessionService::~SessionService() {
  base::UmaHistogramCounts100("SessionRestore.UnrecoverableWriteErrorCount",
                              unrecoverable_write_error_count_);

  // This must be called from SessionService because Save() calls back into
  // SessionService, which will have been destructed already if we try to
  // do this in SessionServiceBase.
  command_storage_manager()->Save();

  DestroyCommandStorageManager();

  // Certain code paths explicitly destroy the SessionService as part of
  // shutdown.
  if (!did_log_exit_)
    LogExitEvent();
}

// static
bool SessionService::IsRelevantWindowType(
    sessions::SessionWindow::WindowType window_type) {
  return (window_type == sessions::SessionWindow::TYPE_NORMAL) ||
         (window_type == sessions::SessionWindow::TYPE_POPUP);
}

bool SessionService::ShouldRestore(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS)
  // Do not restore browser window in the kiosk session.
  if (chromeos::IsKioskSession()) {
    return false;
  }
#endif

  // ChromeOS and OSX have different ideas of application lifetime than
  // the other platforms.
  // On ChromeOS opening a new window should never start a new session.
#if BUILDFLAG(IS_CHROMEOS)
  // On Chrome OS, the ash-chrome browser is not launched automatically during
  // the system startup phase. The lacros-chrome browser may or may not be
  // launched automatically during system startup. When either the ash-chrome or
  // lacros-chrome browser is created or launched by users, sessions might be
  // restored based on the startup setting.

  // If there are other browser windows, or during the restoring process, or
  // restore from crash, or should not restore for `browser`, sessions should
  // not be restored.
  if (SessionRestore::IsRestoring(profile()) || has_open_trackable_browsers_ ||
      HasPendingUncleanExit(profile()) ||
      (browser && !browser->should_trigger_session_restore())) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Restore should trigger for lacros-chrome if handling a restart or if
  // currently processing a full restore.
  auto* primary_user_profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetPrimaryUserProfilePath());
  if (StartupBrowserCreator::WasRestarted()) {
    return true;
  }
  if (primary_user_profile &&
      BrowserLauncher::GetForProfile(primary_user_profile)
          ->is_launching_for_last_opened_profiles()) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // If the on startup setting is not restore, sessions should not be
  // restored.
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(profile()->GetPrefs());
  if (!pref.ShouldRestoreLastSession()) {
    return false;
  }

  if (!browser)
    return true;

  // App windows should not be restored.
  auto window_type = WindowTypeForBrowserType(browser->type());
  if (window_type == sessions::SessionWindow::TYPE_APP ||
      window_type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return false;
  }

  // If the browser does not have a `restore_id`, then we restore the session.
  return browser->create_params().restore_id == Browser::kDefaultRestoreId;
#else
  if (!has_open_trackable_browsers_ &&
      !StartupBrowserCreator::InSynchronousProfileLaunch() &&
      !SessionRestore::IsRestoring(profile())
#if BUILDFLAG(IS_MAC)
      // On OSX, a new window should not start a new session if it was opened
      // from the dock or the menubar.
      && !app_controller_mac::IsOpeningNewWindow()
#endif  // BUILDFLAG(IS_MAC)
  ) {
    return true;
  }
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool SessionService::RestoreIfNecessary(const StartupTabs& startup_tabs,
                                        bool restore_apps) {
  return RestoreIfNecessary(startup_tabs, nullptr, restore_apps);
}

void SessionService::MoveCurrentSessionToLastSession() {
  DCHECK(is_saving_enabled());
  pending_tab_close_ids_.clear();
  window_closing_ids_.clear();
  pending_window_close_ids_.clear();

  command_storage_manager()->MoveCurrentSessionToLastSession();
  ScheduleResetCommands();
}

void SessionService::DeleteLastSession() {
  command_storage_manager()->DeleteLastSession();
  ++count_delete_last_session_for_testing_;
}

void SessionService::SetTabGroup(SessionID window_id,
                                 SessionID tab_id,
                                 std::optional<tab_groups::TabGroupId> group) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Tabs get ungrouped as they close. However, if the whole window is closing
  // tabs should stay in their groups. So, ignore this call in that case.
  if (base::Contains(pending_window_close_ids_, window_id) ||
      base::Contains(window_closing_ids_, window_id))
    return;

  ScheduleCommand(sessions::CreateTabGroupCommand(tab_id, std::move(group)));
}

void SessionService::SetTabGroupMetadata(
    SessionID window_id,
    const tab_groups::TabGroupId& group_id,
    const tab_groups::TabGroupVisualData* visual_data,
    std::optional<std::string> saved_guid) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // Any group metadata changes happening in a closing window can be ignored.
  if (base::Contains(pending_window_close_ids_, window_id) ||
      base::Contains(window_closing_ids_, window_id))
    return;

  ScheduleCommand(sessions::CreateTabGroupMetadataUpdateCommand(
      group_id, visual_data, std::move(saved_guid)));
}

void SessionService::AddTabExtraData(SessionID window_id,
                                     SessionID tab_id,
                                     const char* key,
                                     const std::string& data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateAddTabExtraDataCommand(tab_id, key, data));
}

void SessionService::AddWindowExtraData(SessionID window_id,
                                        const char* key,
                                        const std::string& data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateAddWindowExtraDataCommand(window_id, key, data));
}

void SessionService::TabClosed(SessionID window_id, SessionID tab_id) {
  if (!tab_id.id())
    return;  // Happens when the tab is replaced.

  if (!ShouldTrackChangesToWindow(window_id))
    return;

  auto i = tab_to_available_range()->find(tab_id);
  if (i != tab_to_available_range()->end())
    tab_to_available_range()->erase(i);

  if (find(pending_window_close_ids_.begin(), pending_window_close_ids_.end(),
           window_id) != pending_window_close_ids_.end()) {
    // Tab is in last window and the window is being closed. Don't commit it
    // immediately, instead add it to the list of tabs to close. If the user
    // creates another window, the close is committed.
    // This is necessary to ensure the session is properly stored when closing
    // the final window.
    pending_tab_close_ids_.insert(tab_id);
  } else {
    // If an individual tab is being closed or a secondary window is being
    // closed, just mark the tab as closed now.
    ScheduleCommand(sessions::CreateTabClosedCommand(tab_id));
    if ((find(window_closing_ids_.begin(), window_closing_ids_.end(),
              window_id) == window_closing_ids_.end()) &&
        IsOnlyOneTabLeft()) {
      // This is the last tab in the last tabbed browser.
      has_open_trackable_browsers_ = false;
    }
  }
}

void SessionService::WindowOpened(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  RestoreIfNecessary(StartupTabs(), browser, /* restore_apps */ false);
  SetWindowType(browser->session_id(), browser->type());
  SetWindowAppName(browser->session_id(), browser->app_name());
  SetWindowUserTitle(browser->session_id(), browser->user_title());

  // Save a browser workspace after window is created in `Browser()`.
  // Bento desks restore feature in ash requires this line to restore correctly
  // after creating a new browser window in a particular desk.
  SetWindowWorkspace(browser->session_id(), browser->window()->GetWorkspace());
  SetWindowVisibleOnAllWorkspaces(
      browser->session_id(), browser->window()->IsVisibleOnAllWorkspaces());
}

void SessionService::WindowClosing(SessionID window_id) {
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
        ProfileAttributesEntry* entry =
            profile_manager->GetProfileAttributesStorage()
                .GetProfileAttributesWithPath(profile()->GetPath());
        use_pending_close = entry && entry->IsSigninRequired();
      }
    }
  }
  if (use_pending_close) {
    LogExitEvent();
    pending_window_close_ids_.insert(window_id);
  } else {
    window_closing_ids_.insert(window_id);
  }
}

void SessionService::WindowClosed(SessionID window_id) {
  windows_tracking()->erase(window_id);
  last_selected_tab_in_window()->erase(window_id);

  if (window_closing_ids_.find(window_id) != window_closing_ids_.end()) {
    window_closing_ids_.erase(window_id);
    ScheduleCommand(sessions::CreateWindowClosedCommand(window_id));
  } else if (pending_window_close_ids_.find(window_id) ==
             pending_window_close_ids_.end()) {
    // We'll hit this if user closed the last tab in a window.
    has_open_trackable_browsers_ = HasOpenTrackableBrowsers(window_id);
    if (!has_open_trackable_browsers_) {
      LogExitEvent();
      pending_window_close_ids_.insert(window_id);
    } else {
      ScheduleCommand(sessions::CreateWindowClosedCommand(window_id));
    }
  }
}

void SessionService::SetWindowType(SessionID window_id, Browser::Type type) {
  sessions::SessionWindow::WindowType window_type =
      WindowTypeForBrowserType(type);
  if (!ShouldRestoreWindowOfType(window_type))
    return;

  windows_tracking()->insert(window_id);

  // The user created a new tabbed browser with our profile. Commit any
  // pending closes.
  CommitPendingCloses();

  has_open_trackable_browsers_ = true;
  move_on_new_browser_ = true;

  ScheduleCommand(CreateSetWindowTypeCommand(window_id, window_type));
}

void SessionService::SetWindowUserTitle(SessionID window_id,
                                        const std::string& user_title) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(
      sessions::CreateSetWindowUserTitleCommand(window_id, user_title));
}

void SessionService::OnErrorWritingSessionCommands() {
  // TODO(sky): if `pending_window_close_ids_` is non-empty, then
  // RebuildCommandsIfRequired() will not call ScheduleResetCommands(). This is
  // because a rebuild can't happen (because the browsers in
  // `pending_window_close_ids_` have been deleted). My hope is that this
  // happens seldom enough that we don't need to deal with this case, as it will
  // necessitate some amount of snapshotting in memory when a window is closing.
  // The histogram should give us an idea of how often this happens in practice.
  const bool unrecoverable_write_error = !pending_window_close_ids_.empty();
  if (unrecoverable_write_error)
    ++unrecoverable_write_error_count_;
  LogSessionServiceWriteErrorEvent(profile(), unrecoverable_write_error);
  set_rebuild_on_next_save(true);
  RebuildCommandsIfRequired();
}

void SessionService::SetTabUserAgentOverride(
    SessionID window_id,
    SessionID tab_id,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetTabUserAgentOverrideCommand(
      tab_id, user_agent_override));
}

Browser::Type SessionService::GetDesiredBrowserTypeForWebContents() {
  return Browser::Type::TYPE_NORMAL;
}

void SessionService::DidScheduleCommand() {
  if (did_schedule_command_)
    return;
  did_schedule_command_ = true;
  if (is_first_session_service_)
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40196304): for debugging, remove once tracked down
  // source of problem.
  // A command has been scheduled for a SessionService other than the first.
  // Recreating the SessionService happens if shutdown is canceled, which is
  // valid, but bugs seem to indicate we are getting here in scenarios we don't
  // expect. This debug code is attempting to identify how that is happening.
  const bool shutdown_started = browser_shutdown::HasShutdownStarted();
  base::debug::Alias(&shutdown_started);
  base::debug::DumpWithoutCrashing();
#endif
}

bool SessionService::ShouldRestoreWindowOfType(
    sessions::SessionWindow::WindowType window_type) const {
  // TYPE_APP and TYPE_APP_POPUP are handled by app_session_service.
  return IsRelevantWindowType(window_type);
}

bool SessionService::RestoreIfNecessary(const StartupTabs& startup_tabs,
                                        Browser* browser,
                                        bool restore_apps) {
  if (ShouldRestore(browser)) {
    // We're going from no tabbed browsers to a tabbed browser (and not in
    // process startup), restore the last session.
    if (move_on_new_browser_ && is_saving_enabled()) {
      // Make the current session the last.
      MoveCurrentSessionToLastSession();
      move_on_new_browser_ = false;
    }
    SessionStartupPref pref = StartupBrowserCreator::GetSessionStartupPref(
        *base::CommandLine::ForCurrentProcess(), profile());
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfileIfExisting(profile());
    if (pref.ShouldRestoreLastSession() &&
        (!tab_restore_service || !tab_restore_service->IsRestoring())) {
      SessionRestore::RestoreSession(
          profile(), browser,
          SessionRestore::RESTORE_BROWSER |
              (browser ? 0 : SessionRestore::ALWAYS_CREATE_TABBED_BROWSER) |
              (restore_apps ? SessionRestore::RESTORE_APPS : 0),
          startup_tabs);
      return true;
    }
#if BUILDFLAG(IS_CHROMEOS)
  } else if (HasPendingUncleanExit(profile())) {
    if (!browser) {
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
      // Currently the kNoStartupWindow flag is set when lacros-chrome is
      // launched with crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow.
      // The intention is to prevent lacros-chrome from launching a window
      // during startup. However this flag remains set throughout the whole
      // execution of Chrome.
      // This leads to issues since SessionService uses LaunchBrowser() here to
      // create the first Browser window when the Browser icon is clicked on
      // the shelf. In this case kNoStartupWindow will prevent the Browser
      // window from ever launching.
      // As a temporary workaround remove the kNoStartupWindow switch from the
      // command line when launching the Browser window from
      // RestoreIfNecessary().
      base::CommandLine lacros_command_line =
          base::CommandLine(*base::CommandLine::ForCurrentProcess());
      lacros_command_line.RemoveSwitch(switches::kNoStartupWindow);
      command_line = &lacros_command_line;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

      // If 'browser' is null, call StartupBrowserCreator to create a new
      // browser instance.
      StartupBrowserCreator browser_creator;
      browser_creator.LaunchBrowser(*command_line, profile(), base::FilePath(),
                                    chrome::startup::IsProcessStartup::kYes,
                                    chrome::startup::IsFirstRun::kNo,
                                    /*restore_tabbed_browser=*/true);
      return true;
    } else {
      // If 'browser' is not null, show the crash bubble in the current browser
      // instance.
      SessionCrashedBubble::ShowIfNotOffTheRecordProfile(
          browser, /*skip_tab_checking=*/true);
      AddLaunchedProfile(profile());
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  return false;
}

void SessionService::BuildCommandsForTab(
    SessionID window_id,
    WebContents* tab,
    int index_in_window,
    std::optional<tab_groups::TabGroupId> group,
    bool is_pinned,
    IdToRange* tab_to_available_range) {
  DCHECK(is_saving_enabled());
  SessionServiceBase::BuildCommandsForTab(window_id, tab, index_in_window,
                                          group, is_pinned,
                                          tab_to_available_range);

  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(tab);
  SessionID session_id(session_tab_helper->session_id());

  const blink::UserAgentOverride& ua_override = tab->GetUserAgentOverride();

  if (!ua_override.ua_string_override.empty()) {
    sessions::SerializedUserAgentOverride serialized_ua_override;
    serialized_ua_override.ua_string_override = ua_override.ua_string_override;
    serialized_ua_override.opaque_ua_metadata_override =
        blink::UserAgentMetadata::Marshal(ua_override.ua_metadata_override);

    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateSetTabUserAgentOverrideCommand(session_id,
                                                       serialized_ua_override));
  }

  if (group.has_value()) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateTabGroupCommand(session_id, std::move(group)));
  }

#if defined(TOOLKIT_VIEWS)
  std::optional<std::pair<std::string, std::string>> tab_restore_data =
      side_search::MaybeGetSideSearchTabRestoreData(tab);
  if (tab_restore_data.has_value()) {
    command_storage_manager()->AppendRebuildCommand(
        sessions::CreateAddTabExtraDataCommand(
            session_id, tab_restore_data->first, tab_restore_data->second));
  }
#endif  // defined(TOOLKIT_VIEWS)
}

void SessionService::ScheduleResetCommands() {
  DCHECK(is_saving_enabled());
  command_storage_manager()->set_pending_reset(true);
  command_storage_manager()->ClearPendingCommands();
  tab_to_available_range()->clear();
  windows_tracking()->clear();
  last_selected_tab_in_window()->clear();
  set_rebuild_on_next_save(false);
  BuildCommandsFromBrowsers(tab_to_available_range(), windows_tracking());
  if (!windows_tracking()->empty()) {
    // We're lazily created on startup and won't get an initial batch of
    // SetWindowType messages. Set these here to make sure our state is correct.
    has_open_trackable_browsers_ = true;
    move_on_new_browser_ = true;
  }
  command_storage_manager()->StartSaveTimer();
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

  RemoveExitEvent();
}

bool SessionService::IsOnlyOneTabLeft() const {
  if (profile()->AsTestingProfile())
    return is_only_one_tab_left_for_test_;

  int window_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
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

bool SessionService::HasOpenTrackableBrowsers(SessionID window_id) const {
  if (profile()->AsTestingProfile())
    return has_open_trackable_browser_for_test_;

  for (Browser* browser : *BrowserList::GetInstance()) {
    const SessionID browser_id = browser->session_id();
    if (browser_id != window_id &&
        window_closing_ids_.find(browser_id) == window_closing_ids_.end() &&
        ShouldTrackBrowser(browser)) {
      return true;
    }
  }
  return false;
}

void SessionService::RebuildCommandsIfRequired() {
  if (rebuild_on_next_save() && pending_window_close_ids_.empty() &&
      is_saving_enabled()) {
    ScheduleResetCommands();
  }
}

void SessionService::OnClosingAllBrowsersChanged(bool closing) {
  if (closing)
    LogExitEvent();
}

void SessionService::LogExitEvent() {
  // If there are pending closes, then we have already logged the exit.
  if (!pending_window_close_ids_.empty())
    return;

  RemoveExitEvent();
  int browser_count = 0;
  int tab_count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile()) {
      ++browser_count;
      tab_count += browser->tab_strip_model()->count();
    }
  }
  did_log_exit_ = true;
  LogSessionServiceExitEvent(profile(), browser_count, tab_count,
                             is_first_session_service_, did_schedule_command_);
}

void SessionService::RemoveExitEvent() {
  if (!did_log_exit_)
    return;

  RemoveLastSessionServiceEventOfType(profile(),
                                      SessionServiceEventLogType::kExit);
  did_log_exit_ = false;
}
