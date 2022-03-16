// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/termination_notification.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"  // nogncheck
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"
#endif

namespace browser_shutdown {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
chromeos::UpdateEngineClient* GetUpdateEngineClient() {
  DCHECK(chromeos::DBusThreadManager::IsInitialized());
  auto* update_engine_client =
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
  DCHECK(update_engine_client);
  return update_engine_client;
}
#endif

}  // namespace

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static bool notified = false;
  // Return if a shutdown request has already been sent.
  if (notified)
    return;
  notified = true;
#endif

  if (fast_path)
    NotifyAppTerminating();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::PowerPolicyController::IsInitialized())
    chromeos::PowerPolicyController::Get()->NotifyChromeIsExiting();

  // Reboot if an update has been applied.
  if (UpdatePending() || reboot_policy == RebootPolicy::kForceReboot) {
    GetUpdateEngineClient()->RebootAfterUpdate();
    return;
  }

  // Signal session manager to stop the session if Chrome has initiated an
  // attempt to do so.
  if (chrome::IsAttemptingShutdown() && ash::SessionTerminationManager::Get()) {
    ash::SessionTerminationManager::Get()->StopSession(
        login_manager::SessionStopReason::REQUEST_FROM_SESSION_MANAGER);
  }
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool UpdatePending() {
  if (!chromeos::DBusThreadManager::IsInitialized())
    return false;

  return GetUpdateEngineClient()->GetLastStatus().current_operation() ==
         update_engine::Operation::UPDATED_NEED_REBOOT;
}
#endif

}  // namespace browser_shutdown
