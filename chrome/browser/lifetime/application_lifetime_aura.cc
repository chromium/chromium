// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/lifetime/application_lifetime_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
#include "chrome/browser/notifications/notification_ui_manager.h"
#endif

namespace chrome {

void HandleAppExitingForPlatform() {
  // Close all non browser windows now. Those includes notifications
  // and windows created by Ash (launcher, background, etc).

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::Shell::HasInstance()) {
    // Releasing the capture will close any menus that might be open:
    // http://crbug.com/134472
    aura::client::GetCaptureClient(ash::Shell::GetPrimaryRootWindow())
        ->SetCapture(nullptr);
  }
#endif

#if BUILDFLAG(ENABLE_CHROME_NOTIFICATIONS)
  // This clears existing notifications from the message center and their
  // associated ScopedKeepAlives. Chrome OS doesn't use ScopedKeepAlives for
  // notifications.
  if (auto* notification_ui_manager =
          g_browser_process->notification_ui_manager();
      notification_ui_manager) {
    notification_ui_manager->StartShutdown();
  }
#endif

  views::Widget::CloseAllSecondaryWidgets();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableZeroBrowsersOpenForTests)) {
    // App is exiting, release the keep alive on behalf of Aura Shell.
    g_browser_process->platform_part()->UnregisterKeepAlive();
    // Make sure we have notified the session manager that we are exiting.
    // This might be called from FastShutdown() or CloseAllBrowsers(), but not
    // if something prevents a browser from closing before SetTryingToQuit()
    // gets called (e.g. browser->TabsNeedBeforeUnloadFired() is true).
    // NotifyAndTerminate does nothing if called more than once.
    browser_shutdown::NotifyAppTerminating();
    StopSession();
  }
#endif
}

}  // namespace chrome
