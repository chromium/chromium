// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_webui.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/widget/widget.h"

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

void LoginDisplayWebUI::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

bool LoginDisplayWebUI::IsSigninInProgress() const {
  return delegate_->IsSigninInProgress();
}

bool LoginDisplayWebUI::IsUserSigninCompleted() const {
  return is_signin_completed();
}

void LoginDisplayWebUI::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetAutoLoginTimer();
}

}  // namespace ash
