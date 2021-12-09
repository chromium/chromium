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

  void Show(const std::u16string& text) {
    GetController()->UpdateBubble(/*visible=*/true, text);
  }

  void Hide() { GetController()->UpdateBubble(/*visible=*/false, u""); }

  DictationBubbleView* GetView() {
    return GetController()->dictation_bubble_view_;
  }

  bool IsBubbleVisible() { return GetController()->widget_->IsVisible(); }

  std::u16string GetBubbleText() {
    return GetController()->dictation_bubble_view_->GetTextForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DictationBubbleControllerTest, ShowAndHide) {
  EXPECT_FALSE(GetView());
  Show(u"Testing");
  EXPECT_TRUE(GetView());
  EXPECT_TRUE(IsBubbleVisible());
  Hide();
  EXPECT_TRUE(GetView());
  EXPECT_FALSE(IsBubbleVisible());
}

TEST_F(DictationBubbleControllerTest, LabelText) {
  EXPECT_FALSE(GetView());
  Show(u"Testing");
  EXPECT_EQ(u"Testing", GetBubbleText());
}

}  // namespace ash
