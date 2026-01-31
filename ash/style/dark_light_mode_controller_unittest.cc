// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_controller_impl.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using DarkLightModeControllerTest = NoSessionAshTestBase;

// Tests the color mode in non-active user sessions.
TEST_F(DarkLightModeControllerTest, ColorModeInNonActiveUserSessions) {
  auto* client = GetSessionControllerClient();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();

  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  // When dark/light mode is enabled. Color mode in non-active user sessions
  // (e.g, login page) should be DARK.
  auto* active_user_pref_service =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_FALSE(active_user_pref_service);
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  // But color mode should be LIGHT in OOBE.
  auto* dispatcher = Shell::Get()->login_screen_controller()->data_dispatcher();
  client->SetSessionState(session_manager::SessionState::OOBE);
  dispatcher->NotifyOobeDialogState(OobeDialogState::USER_CREATION);
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());

  client->SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  dispatcher->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(dark_light_mode_controller->IsDarkModeEnabled());

  dispatcher->NotifyOobeDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_FALSE(dark_light_mode_controller->IsDarkModeEnabled());
}

}  // namespace ash
