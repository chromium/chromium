// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr char kTestString[] = "test";

using AssistantTextElementViewTest = AshTestBase;

TEST_F(AssistantTextElementViewTest, DarkAndLightTheme) {
  auto* color_provider = AshColorProvider::Get();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  AssistantTextElementView* text_element_view = widget->SetContentsView(
      std::make_unique<AssistantTextElementView>(kTestString));

  views::Label* label =
      static_cast<views::Label*>(text_element_view->children().at(0));

  EXPECT_EQ(label->GetEnabledColor(),
            color_provider->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(label->GetEnabledColor(),
            color_provider->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary));
}

}  // namespace
}  // namespace ash
