// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_

#include "chrome/browser/ash/login/ui/login_display.h"
#include "components/user_manager/user.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace ash {

// WebUI-based login UI implementation.
class LoginDisplayWebUI : public LoginDisplay, public ui::UserActivityObserver {
 public:
  LoginDisplayWebUI();

  LoginDisplayWebUI(const LoginDisplayWebUI&) = delete;
  LoginDisplayWebUI& operator=(const LoginDisplayWebUI&) = delete;

  ~LoginDisplayWebUI() override;

  // LoginDisplay implementation:
  void Init(const user_manager::UserList& users, bool show_guest) override;
  void SetUIEnabled(bool is_enabled) override;

  // ui::UserActivityDetector implementation:
  void OnUserActivity(const ui::Event* event) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_LOGIN_DISPLAY_WEBUI_H_
