// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

class DictationBubbleControllerTest : public AshTestBase {
 public:
  DictationBubbleControllerTest() = default;
  ~DictationBubbleControllerTest() override = default;
  DictationBubbleControllerTest(const DictationBubbleControllerTest&) = delete;
  DictationBubbleControllerTest& operator=(
      const DictationBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kNotificationsRefresh});

    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
  }

  DictationBubbleController* GetController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetDictationBubbleControllerForTest();
  }

  void Show(DictationBubbleIconType icon,
            const absl::optional<std::u16string>& text,
            const absl::optional<std::vector<DictationBubbleHintType>>& hints) {
    GetController()->UpdateBubble(
        /*visible=*/true, /*icon=*/icon, /*text=*/text, /*hints=*/hints);
  }

  void Hide() {
    GetController()->UpdateBubble(
        /*visible=*/false,
        /*icon=*/DictationBubbleIconType::kHidden,
        /*text=*/std::u16string(),
        /*hints=*/std::vector<DictationBubbleHintType>());
  }

  DictationBubbleView* GetView() {
    return GetController()->dictation_bubble_view_;
  }

  bool IsBubbleVisible() { return GetController()->widget_->IsVisible(); }

  std::u16string GetBubbleText() { return GetView()->GetTextForTesting(); }

  bool IsStandbyViewVisible() {
    return GetView()->IsStandbyViewVisibleForTesting();
  }

  bool IsMacroSucceededImageVisible() {
    return GetView()->IsMacroSucceededImageVisibleForTesting();
  }

  bool IsMacroFailedImageVisible() {
    return GetView()->IsMacroFailedImageVisibleForTesting();
  }

  void HideAndCheckExpectations() {
    Hide();
    EXPECT_TRUE(GetView());
    EXPECT_FALSE(IsBubbleVisible());
    EXPECT_EQ(std::u16string(), GetBubbleText());
    EXPECT_FALSE(IsStandbyViewVisible());
    EXPECT_FALSE(IsMacroSucceededImageVisible());
    EXPECT_FALSE(IsMacroFailedImageVisible());
  }

  SkColor GetLabelBackgroundColor() {
    return GetView()->GetLabelBackgroundColorForTesting();
  }

  SkColor GetLabelTextColor() {
    return GetView()->GetLabelTextColorForTesting();
  }

  std::vector<std::u16string> GetVisibleHints() {
    return GetView()->GetVisibleHintsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DictationBubbleControllerTest, ShowText) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kHidden,
       absl::optional<std::u16string>(u"Testing"),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(u"Testing", GetBubbleText());
  EXPECT_FALSE(IsStandbyViewVisible());
  EXPECT_FALSE(IsMacroSucceededImageVisible());
  EXPECT_FALSE(IsMacroFailedImageVisible());

  HideAndCheckExpectations();
}

TEST_F(DictationBubbleControllerTest, ShowStandbyImage) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kStandby, absl::optional<std::u16string>(),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(std::u16string(), GetBubbleText());
  EXPECT_TRUE(IsStandbyViewVisible());
  EXPECT_FALSE(IsMacroSucceededImageVisible());
  EXPECT_FALSE(IsMacroFailedImageVisible());

  HideAndCheckExpectations();
}

TEST_F(DictationBubbleControllerTest, ShowMacroSuccessImage) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kMacroSuccess,
       absl::optional<std::u16string>(u"Macro successfull"),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(u"Macro successfull", GetBubbleText());
  EXPECT_FALSE(IsStandbyViewVisible());
  EXPECT_TRUE(IsMacroSucceededImageVisible());
  EXPECT_FALSE(IsMacroFailedImageVisible());

  HideAndCheckExpectations();
}

TEST_F(DictationBubbleControllerTest, ShowMacroFailImage) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kMacroFail,
       absl::optional<std::u16string>(u"Macro failed"),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(u"Macro failed", GetBubbleText());
  EXPECT_FALSE(IsStandbyViewVisible());
  EXPECT_FALSE(IsMacroSucceededImageVisible());
  EXPECT_TRUE(IsMacroFailedImageVisible());

  HideAndCheckExpectations();
}

// Verifies that the bubble UI respects the dark mode setting. For convenience
// purposes, we perform checks on the label's text and background color.
TEST_F(DictationBubbleControllerTest, DarkMode) {
  // Enable dark mode feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kDarkLightMode);
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetPrimaryUserPrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  // Show bubble UI.
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kHidden,
       absl::optional<std::u16string>(u"Testing"),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(u"Testing", GetBubbleText());
  const SkColor initial_text_color = GetLabelTextColor();
  const SkColor initial_background_color = GetLabelBackgroundColor();
  auto* color_provider = GetView()->GetColorProvider();
  EXPECT_EQ(initial_text_color,
            color_provider->GetColor(kColorAshTextColorPrimary));
  EXPECT_EQ(initial_background_color,
            color_provider->GetColor(ui::kColorDialogBackground));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  const bool dark_mode_status = dark_light_mode_controller->IsDarkModeEnabled();
  ASSERT_NE(initial_dark_mode_status, dark_mode_status);

  // Since the color mode has been updated, we need to get the refreshed color
  // provider.
  color_provider = GetView()->GetColorProvider();

  // Verify that the text and background colors changed and still have the
  // right colors according to the color modes.
  const SkColor text_color = GetLabelTextColor();
  const SkColor background_color = GetLabelBackgroundColor();
  EXPECT_NE(text_color, initial_text_color);
  EXPECT_EQ(text_color, color_provider->GetColor(kColorAshTextColorPrimary));
  EXPECT_NE(background_color, initial_background_color);
  EXPECT_EQ(background_color,
            color_provider->GetColor(ui::kColorDialogBackground));

  HideAndCheckExpectations();
}

TEST_F(DictationBubbleControllerTest, Hints) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kStandby, absl::optional<std::u16string>(),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());

  EXPECT_TRUE(GetVisibleHints().size() == 0);

  HideAndCheckExpectations();
}

// Verifies that the UI can be hidden before being shown.
TEST_F(DictationBubbleControllerTest, HideBeforeShow) {
  HideAndCheckExpectations();

  EXPECT_TRUE(GetView());
  Show(DictationBubbleIconType::kStandby, absl::optional<std::u16string>(),
       absl::optional<std::vector<DictationBubbleHintType>>());
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());

  HideAndCheckExpectations();
}

}  // namespace ash
