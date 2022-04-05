// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/login_types.h"
#include "ash/style/ash_color_provider.h"

#include "ash/session/test_session_controller_client.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

using AshColorProviderTest = LoginTestBase;

// Tests the color mode in non-active user sessions.
TEST_F(AshColorProviderTest, ColorModeInNonActiveUserSessions) {
  auto* client = GetSessionControllerClient();
  auto* color_provider = AshColorProvider::Get();

  // When dark/light mode is enabled. Color mode in non-active user sessions
  // (e.g, login page) should be DARK, but LIGHT while in OOBE.
  base::test::ScopedFeatureList enable_dark_light;
  enable_dark_light.InitAndEnableFeature(chromeos::features::kDarkLightMode);
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());

  client->SetSessionState(session_manager::SessionState::OOBE);
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::USER_CREATION);
  EXPECT_FALSE(color_provider->IsDarkModeEnabled());

  client->SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::HIDDEN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());

  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::GAIA_SIGNIN);
  EXPECT_FALSE(color_provider->IsDarkModeEnabled());

  // When dark/light mode is disabled. Color mode in non-active user sessions
  // (e.g, login page) should still be DARK.
  base::test::ScopedFeatureList disable_dark_light;
  disable_dark_light.InitAndDisableFeature(chromeos::features::kDarkLightMode);
  client->SetSessionState(session_manager::SessionState::UNKNOWN);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
  client->SetSessionState(session_manager::SessionState::OOBE);
  DataDispatcher()->NotifyOobeDialogState(OobeDialogState::USER_CREATION);
  EXPECT_TRUE(color_provider->IsDarkModeEnabled());
}

}  // namespace ash
