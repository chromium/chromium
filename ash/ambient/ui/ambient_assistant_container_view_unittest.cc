// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_assistant_container_view.h"

#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "ui/views/view.h"

namespace ash {

class AmbientAssistantContainerViewTest : public AmbientAshTestBase {
 public:
  AmbientAssistantContainerViewTest() : AmbientAshTestBase() {}
  ~AmbientAssistantContainerViewTest() override = default;

  // AmbientAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::assistant::features::kEnableAmbientAssistant);
    AmbientAshTestBase::SetUp();
    UpdateDisplay("800x600");
    ShowAmbientScreen();
  }

  void TearDown() override {
    CloseAmbientScreen();
    AmbientAshTestBase::TearDown();
  }

  AmbientAssistantContainerView* GetAmbientAssistantContainerView() {
    return static_cast<AmbientAssistantContainerView*>(
        GetContainerView()->GetViewByID(
            AmbientViewID::kAmbientAssistantContainerView));
  }

  views::View* GetInnerContainer() {
    const auto& children = GetAmbientAssistantContainerView()->children();
    EXPECT_EQ(children.size(), 1u);
    return children.front();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AmbientAssistantContainerViewTest, LayoutAmbientContainerView) {
  GetAmbientAssistantContainerView()->SetVisible(true);

  auto* inner_container = GetInnerContainer();

  // Should be 100% width and fixed height.
  EXPECT_EQ(inner_container->GetBoundsInScreen(),
            gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/800, /*height=*/128));

  UpdateDisplay("1920x1080");

  // Should expand to still be 100% width.
  EXPECT_EQ(inner_container->GetBoundsInScreen(),
            gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/1920, /*height=*/128));
}

}  // namespace ash
