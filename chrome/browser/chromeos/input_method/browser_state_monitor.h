// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {
namespace input_method {

// Translates notifications from the browser (not logged in, logged in, etc.),
// into InputMethodManager::UISessionState transitions.
class BrowserStateMonitor : public session_manager::SessionManagerObserver,
                            public content::NotificationObserver {
 public:
  // Constructs a monitor that will invoke the given observer callback whenever
  // the InputMethodManager::UISessionState changes. Assumes that the current
  // ui_session_ is STATE_LOGIN_SCREEN. |observer| may be null.
  explicit BrowserStateMonitor(
      const base::Callback<void(InputMethodManager::UISessionState)>& observer);
  ~BrowserStateMonitor() override;

  InputMethodManager::UISessionState ui_session() const { return ui_session_; }

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void SetUiSessionState(InputMethodManager::UISessionState ui_session);

  base::Callback<void(InputMethodManager::UISessionState)> observer_;
  InputMethodManager::UISessionState ui_session_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserStateMonitor);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_BROWSER_STATE_MONITOR_H_
