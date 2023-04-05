// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/app_session_service.h"

#include <algorithm>
#include <set>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_common_utils.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_types.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/app_controller_mac.h"
#endif

using content::NavigationEntry;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

AppSessionService::AppSessionService(Profile* profile)
    : SessionServiceBase(profile,
                         SessionServiceBase::SessionServiceType::kAppRestore) {}

AppSessionService::~AppSessionService() {
  // The BrowserList should outlive the SessionService since it's static and
  // the SessionService is a KeyedService.
  // BrowserList is subscribed to by SessionServiceBase's constructor.
  BrowserList::RemoveObserver(this);
  command_storage_manager()->Save();

  DestroyCommandStorageManager();
}

void AppSessionService::TabClosed(SessionID window_id, SessionID tab_id) {
  if (!tab_id.id())
    return;  // Happens when the tab is replaced.

  if (!ShouldTrackChangesToWindow(window_id))
    return;

  auto i = tab_to_available_range()->find(tab_id);
  if (i != tab_to_available_range()->end())
    tab_to_available_range()->erase(i);

  // If an individual tab is being closed or a secondary window is being
  // closed, just mark the tab as closed now.
  ScheduleCommand(sessions::CreateTabClosedCommand(tab_id));
}

void AppSessionService::WindowOpened(Browser* browser) {
  if (!ShouldTrackBrowser(browser))
    return;

  SetWindowType(browser->session_id(), browser->type());
  SetWindowAppName(browser->session_id(), browser->app_name());

  // Save a browser workspace after window is created in `Browser()`.
  // Bento desks restore feature in ash requires this line to restore correctly
  // after creating a new browser window in a particular desk.
  SetWindowWorkspace(browser->session_id(), browser->window()->GetWorkspace());
}

void AppSessionService::WindowClosing(SessionID window_id) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  // If Chrome is closed immediately after a history deletion, we have to
  // rebuild commands before this window is closed, otherwise these tabs would
  // be lost.
  RebuildCommandsIfRequired();
}

void AppSessionService::WindowClosed(SessionID window_id) {
  if (!ShouldTrackChangesToWindow(window_id)) {
    return;
  }

  windows_tracking()->erase(window_id);

  tab_to_available_range()->erase(window_id);

  ScheduleCommand(sessions::CreateWindowClosedCommand(window_id));
}

void AppSessionService::SetWindowType(SessionID window_id, Browser::Type type) {
  sessions::SessionWindow::WindowType window_type =
      WindowTypeForBrowserType(type);
  if (!ShouldRestoreWindowOfType(window_type))
    return;

  windows_tracking()->insert(window_id);

  ScheduleCommand(CreateSetWindowTypeCommand(window_id, window_type));
}

Browser::Type AppSessionService::GetDesiredBrowserTypeForWebContents() {
  return Browser::Type::TYPE_APP;
}

bool AppSessionService::ShouldRestoreWindowOfType(
    sessions::SessionWindow::WindowType window_type) const {
  if (window_type == sessions::SessionWindow::TYPE_APP ||
      window_type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return true;
  }

  return false;
}

void AppSessionService::ScheduleResetCommands() {
  command_storage_manager()->set_pending_reset(true);
  command_storage_manager()->ClearPendingCommands();
  tab_to_available_range()->clear();
  windows_tracking()->clear();
  set_rebuild_on_next_save(false);
  BuildCommandsFromBrowsers(tab_to_available_range(), windows_tracking());
  command_storage_manager()->StartSaveTimer();
}

void AppSessionService::RebuildCommandsIfRequired() {
  if (rebuild_on_next_save())
    ScheduleResetCommands();
}
