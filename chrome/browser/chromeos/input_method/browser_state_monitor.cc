// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/browser_state_monitor.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/ime/chromeos/input_method_delegate.h"
#include "ui/base/ime/chromeos/input_method_util.h"

namespace chromeos {
namespace input_method {

BrowserStateMonitor::BrowserStateMonitor(
    const base::Callback<void(InputMethodManager::UISessionState)>& observer)
    : observer_(observer), ui_session_(InputMethodManager::STATE_LOGIN_SCREEN) {
  session_manager::SessionManager::Get()->AddObserver(this);
  // We should not use ALL_BROWSERS_CLOSING here since logout might be cancelled
  // by JavaScript after ALL_BROWSERS_CLOSING is sent (crosbug.com/11055).
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());

  if (observer_)
    observer_.Run(ui_session_);
}

BrowserStateMonitor::~BrowserStateMonitor() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void BrowserStateMonitor::OnSessionStateChanged() {
  // Note: session state changes in the following order.
  //
  // Normal login:
  // 1. State changes to LOGGED_IN_NOT_ACTIVE
  // 2. Preferences::NotifyPrefChanged() is called. preload_engines (which
  //    might change the current input method) and current/previous input method
  //    are sent to the manager.
  // 3. State changes to ACTIVE
  //
  // Chrome crash/restart (after logging in):
  // 1. State *might* change to LOGGED_IN_NOT_ACTIVE
  // 2. State changes to ACTIVE
  // 3. Preferences::NotifyPrefChanged() is called. The same things as above
  //    happen.
  //
  // We have to be careful not to overwrite both local and user prefs when
  // NotifyPrefChanged is called. Note that it does not work to do nothing in
  // InputMethodChanged() between LOGGED_IN_NOT_ACTIVE and ACTIVE because
  // SESSION_STARTED is sent very early on Chrome crash/restart.
  auto session_state = session_manager::SessionManager::Get()->session_state();
  if (session_state == session_manager::SessionState::ACTIVE ||
      session_state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE) {
    SetUiSessionState(InputMethodManager::STATE_BROWSER_SCREEN);
  } else if (session_state == session_manager::SessionState::LOCKED) {
    SetUiSessionState(InputMethodManager::STATE_LOCK_SCREEN);
  }
}

void BrowserStateMonitor::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
  SetUiSessionState(InputMethodManager::STATE_TERMINATING);
}

void BrowserStateMonitor::SetUiSessionState(
    InputMethodManager::UISessionState ui_session) {
  const InputMethodManager::UISessionState old_ui_session = ui_session_;
  ui_session_ = ui_session;
  if (old_ui_session != ui_session_ && !observer_.is_null())
    observer_.Run(ui_session_);
}

}  // namespace input_method
}  // namespace chromeos
