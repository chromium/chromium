// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"

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
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kExperimentalAccessibilityDictationCommands);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
  }

  DictationBubbleController* GetController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetDictationBubbleControllerForTest();
  }

  void Show(DictationBubbleIconType icon,
            const absl::optional<std::u16string>& text) {
    GetController()->UpdateBubble(
        /*visible=*/true, /*icon=*/icon, /*text=*/text);
  }

  void Hide() {
    GetController()->UpdateBubble(/*visible=*/false,
                                  /*icon=*/DictationBubbleIconType::kHidden,
                                  /*text=*/std::u16string());
  }

  DictationBubbleView* GetView() {
    return GetController()->dictation_bubble_view_;
  }

  bool IsBubbleVisible() { return GetController()->widget_->IsVisible(); }

  std::u16string GetBubbleText() {
    return GetController()->dictation_bubble_view_->GetTextForTesting();
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DictationBubbleControllerTest, ShowText) {
  EXPECT_FALSE(GetView());
  Show(DictationBubbleIconType::kHidden,
       absl::optional<std::u16string>(u"Testing"));
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
  Show(DictationBubbleIconType::kStandby, absl::optional<std::u16string>());
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
       absl::optional<std::u16string>(u"Macro successfull"));
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
       absl::optional<std::u16string>(u"Macro failed"));
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  EXPECT_EQ(u"Macro failed", GetBubbleText());
  EXPECT_FALSE(IsStandbyViewVisible());
  EXPECT_FALSE(IsMacroSucceededImageVisible());
  EXPECT_TRUE(IsMacroFailedImageVisible());

  HideAndCheckExpectations();
}

}  // namespace ash
