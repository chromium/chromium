// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_webui.h"

#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

// LoginDisplayWebUI, public: --------------------------------------------------

LoginDisplayWebUI::~LoginDisplayWebUI() {
  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// LoginDisplay implementation: ------------------------------------------------

LoginDisplayWebUI::LoginDisplayWebUI() = default;

void LoginDisplayWebUI::Init(const user_manager::UserList& users,
                             bool show_guest) {
  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && !activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

// ---- Common methods

// ---- Gaia screen methods

// ---- Not yet classified methods

void LoginDisplayWebUI::SetUIEnabled(bool is_enabled) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (host && host->GetWebUILoginView())
    host->GetWebUILoginView()->SetUIEnabled(is_enabled);
}

void LoginDisplayWebUI::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetAutoLoginTimer();
}

}  // namespace ash
