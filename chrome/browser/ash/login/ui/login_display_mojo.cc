// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_mojo.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

LoginDisplayMojo::LoginDisplayMojo() {
  user_manager::UserManager::Get()->AddObserver(this);
}

LoginDisplayMojo::~LoginDisplayMojo() {
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void LoginDisplayMojo::SetUIEnabled(bool is_enabled) {}

void LoginDisplayMojo::OnUserImageChanged(const user_manager::User& user) {
  LoginScreen::Get()->GetModel()->SetAvatarForUser(
      user.GetAccountId(),
      UserSelectionScreen::BuildAshUserAvatarForUser(user));
}

}  // namespace ash
