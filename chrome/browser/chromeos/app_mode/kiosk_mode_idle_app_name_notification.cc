// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/ui/idle_app_name_notification_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos {

namespace {

// The timeout in ms before the message shows up.
const int kIdleAppNameNotificationTimeoutMs = 20 * 60 * 1000;

// The duration of visibility for the message.
const int kMessageVisibilityTimeMs = 3000;

// The anomation time to show / hide the message.
const int kMessageAnimationTimeMs = 200;

// Our global instance of the Kiosk mode message.
KioskModeIdleAppNameNotification* g_kiosk_mode_idle_app_message = NULL;

}  // namespace

// static
void KioskModeIdleAppNameNotification::Initialize() {
  DCHECK(!g_kiosk_mode_idle_app_message);
  g_kiosk_mode_idle_app_message = new KioskModeIdleAppNameNotification();
}

// static
void KioskModeIdleAppNameNotification::Shutdown() {
  if (g_kiosk_mode_idle_app_message) {
    delete g_kiosk_mode_idle_app_message;
    g_kiosk_mode_idle_app_message = NULL;
  }
}

KioskModeIdleAppNameNotification::KioskModeIdleAppNameNotification()
    : show_notification_upon_next_user_activity_(false) {
  // Note: The timeout is currently fixed. If that changes we need to check if
  // the KioskModeSettings were already initialized.
  Setup();
}

KioskModeIdleAppNameNotification::~KioskModeIdleAppNameNotification() {
  ui::UserActivityDetector* user_activity_detector =
      ui::UserActivityDetector::Get();
  if (user_activity_detector && user_activity_detector->HasObserver(this))
    user_activity_detector->RemoveObserver(this);

  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager && power_manager->HasObserver(this))
    power_manager->RemoveObserver(this);
}

void KioskModeIdleAppNameNotification::Setup() {
  DCHECK(user_manager::UserManager::Get()->IsUserLoggedIn());
  Start();
}

void KioskModeIdleAppNameNotification::OnUserActivity(const ui::Event* event) {
  if (show_notification_upon_next_user_activity_) {
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    // The widget only appears on internal displays because the intent is to
    // avoid an app spoofing a password screen on a Chromebook. Customers using
    // external monitors for kiosk don't want this notification to show.
    // See https://crbug.com/369118
    if (display.IsInternal()) {
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      const std::string app_id =
          command_line->GetSwitchValueASCII(::switches::kAppId);
      Profile* profile = ProfileManager::GetActiveUserProfile();
      notification_.reset(new IdleAppNameNotificationView(
          kMessageVisibilityTimeMs, kMessageAnimationTimeMs,
          extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
              app_id)));
    }
    show_notification_upon_next_user_activity_ = false;
  }
  ResetTimer();
}

void KioskModeIdleAppNameNotification::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // When we come back from a system resume we stop the timer and show the
  // message.
  timer_.Stop();
  OnTimeout();
}

void KioskModeIdleAppNameNotification::Start() {
  if (!ui::UserActivityDetector::Get()->HasObserver(this)) {
    ui::UserActivityDetector::Get()->AddObserver(this);
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }
  ResetTimer();
}

void KioskModeIdleAppNameNotification::ResetTimer() {
  if (timer_.IsRunning()) {
    timer_.Reset();
  } else {
    // OneShotTimer destroys the posted task after running it, so Reset()
    // isn't safe to call on a timer that's already fired.
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kIdleAppNameNotificationTimeoutMs),
        base::Bind(&KioskModeIdleAppNameNotification::OnTimeout,
                   base::Unretained(this)));
  }
}

void KioskModeIdleAppNameNotification::OnTimeout() {
  show_notification_upon_next_user_activity_ = true;
}

}  // namespace chromeos
