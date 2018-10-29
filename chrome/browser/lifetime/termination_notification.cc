// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/termination_notification.h"

#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/update_engine_client.h"
#endif

namespace browser_shutdown {

void NotifyAppTerminating() {
  static bool notified = false;
  if (notified)
    return;
  notified = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void NotifyAndTerminate(bool fast_path) {
  NotifyAndTerminate(fast_path, RebootPolicy::kOptionalReboot);
}

void NotifyAndTerminate(bool fast_path, RebootPolicy reboot_policy) {
#if defined(OS_CHROMEOS)
  static bool notified = false;
  // Return if a shutdown request has already been sent.
  if (notified)
    return;
  notified = true;
#endif

  if (fast_path)
    NotifyAppTerminating();

#if defined(OS_CHROMEOS)
  if (chromeos::PowerPolicyController::IsInitialized())
    chromeos::PowerPolicyController::Get()->NotifyChromeIsExiting();

  if (base::SysInfo::IsRunningOnChromeOS()) {
    // If we're on a ChromeOS device, reboot if an update has been applied,
    // or else signal the session manager to log out.
    chromeos::UpdateEngineClient* update_engine_client =
        chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
    if (update_engine_client->GetLastStatus().status ==
            chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT ||
        reboot_policy == RebootPolicy::kForceReboot) {
      update_engine_client->RebootAfterUpdate();
    } else if (chrome::IsAttemptingShutdown()) {
      // Don't ask SessionManager to stop session if the shutdown request comes
      // from session manager.
      chromeos::DBusThreadManager::Get()
          ->GetSessionManagerClient()
          ->StopSession();
    }
  } else {
    if (chrome::IsAttemptingShutdown()) {
      // If running the Chrome OS build, but we're not on the device, act
      // as if we received signal from SessionManager.
      base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                               base::Bind(&chrome::ExitCleanly));
    }
  }
#endif
}

}  // namespace browser_shutdown
