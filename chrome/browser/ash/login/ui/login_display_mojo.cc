// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_mojo.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"

namespace ash {

LoginDisplayMojo::LoginDisplayMojo() {
  user_manager::UserManager::Get()->AddObserver(this);
}

LoginDisplayMojo::~LoginDisplayMojo() {
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void LoginDisplayMojo::OnUserImageChanged(const user_manager::User& user) {
  LoginScreen::Get()->GetModel()->SetAvatarForUser(
      user.GetAccountId(),
      UserSelectionScreen::BuildAshUserAvatarForUser(user));
}

}  // namespace ash
