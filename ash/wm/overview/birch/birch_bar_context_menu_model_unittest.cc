// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"

#include "ash/constants/geolocation_access_level.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"

namespace ash {

using BirchBarContextMenuModelTest = AshTestBase;

TEST_F(BirchBarContextMenuModelTest, WeatherDisabledWhenGeolocationDisabled) {
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kDisallowed);

  BirchBarContextMenuModel model(
      BirchBarController::Get(),
      BirchBarContextMenuModel::Type::kExpandedBarMenu);

  // Find the weather item.
  auto weather_index = model.GetIndexOfCommandId(base::to_underlying(
      BirchBarContextMenuModel::CommandId::kWeatherSuggestions));
  ASSERT_TRUE(weather_index.has_value());

  // The weather item is disabled.
  EXPECT_FALSE(model.IsEnabledAt(weather_index.value()));

  // The item has minor text (which will become a tooltip).
  EXPECT_FALSE(model.GetMinorTextAt(weather_index.value()).empty());
}

TEST_F(BirchBarContextMenuModelTest, WeatherEnabledWhenGeolocationEnabled) {
  SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
      GeolocationAccessLevel::kAllowed);

  BirchBarContextMenuModel model(
      BirchBarController::Get(),
      BirchBarContextMenuModel::Type::kExpandedBarMenu);

  // Find the weather item.
  auto weather_index = model.GetIndexOfCommandId(base::to_underlying(
      BirchBarContextMenuModel::CommandId::kWeatherSuggestions));
  ASSERT_TRUE(weather_index.has_value());

  // The weather item is enabled.
  EXPECT_TRUE(model.IsEnabledAt(weather_index.value()));

  // The item does not have minor text.
  EXPECT_TRUE(model.GetMinorTextAt(weather_index.value()).empty());
}

}  // namespace ash
