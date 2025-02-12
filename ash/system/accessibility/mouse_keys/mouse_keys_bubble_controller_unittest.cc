// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_controller.h"

#include <string_view>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/mouse_keys/mouse_keys_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {

class MouseKeysBubbleControllerTest : public AshTestBase {
 public:
  MouseKeysBubbleControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~MouseKeysBubbleControllerTest() override = default;
  MouseKeysBubbleControllerTest(const MouseKeysBubbleControllerTest&) = delete;
  MouseKeysBubbleControllerTest& operator=(
      const MouseKeysBubbleControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMouseKeys);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(true);
  }

  MouseKeysBubbleController* GetController() const {
    return Shell::Get()
        ->mouse_keys_controller()
        ->GetMouseKeysBubbleControllerForTest();
  }

  void Update(MouseKeysBubbleIconType icon, const std::u16string& text) {
    GetController()->UpdateBubble(/*visible=*/true, icon, text);
  }

  MouseKeysBubbleView* GetView() const {
    return GetController()->mouse_keys_bubble_view_;
  }

  std::u16string_view GetBubbleText() const {
    return GetView()->GetTextForTesting();
  }

  bool IsBubbleVisible() {
    // Add a null check for widget_.
    if (GetController()->widget_ == nullptr) {
      return false;
    }
    return GetController()->widget_->IsVisible();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MouseKeysBubbleControllerTest, LabelText) {
  EXPECT_FALSE(GetView());
  Update(MouseKeysBubbleIconType::kHidden, u"Testing");
  EXPECT_TRUE(GetView());
  EXPECT_EQ(u"Testing", GetBubbleText());

  Update(MouseKeysBubbleIconType::kHidden, u"");
  EXPECT_TRUE(GetView());
  EXPECT_EQ(u"", GetBubbleText());
}

TEST_F(MouseKeysBubbleControllerTest, AccessibleProperties) {
  Update(MouseKeysBubbleIconType::kHidden, u"");
  ui::AXNodeData data;
  GetView()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGenericContainer);
}

TEST_F(MouseKeysBubbleControllerTest, BubbleAutoHides) {
  EXPECT_FALSE(IsBubbleVisible());
  Update(MouseKeysBubbleIconType::kHidden, u"Testing");
  EXPECT_TRUE(IsBubbleVisible());
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(IsBubbleVisible());
}

}  // namespace ash
