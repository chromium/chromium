// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/view_shadow.h"

#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/shadow_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace ash {

using ViewShadowTest = views::ViewsTestBase;

TEST_F(ViewShadowTest, UseShadow) {
  views::View root;
  root.SetPaintToLayer();

  views::View* v1 = root.AddChildView(std::make_unique<views::View>());
  v1->SetPaintToLayer();
  views::View* v2 = root.AddChildView(std::make_unique<views::View>());
  v2->SetPaintToLayer();
  views::View* v3 = root.AddChildView(std::make_unique<views::View>());
  v3->SetPaintToLayer();

  auto shadow = std::make_unique<ViewShadow>(v2, 1);

  ASSERT_EQ(4u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(shadow->shadow()->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[2]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[3]);

  shadow.reset();
  EXPECT_EQ(3u, root.layer()->children().size());
  EXPECT_EQ(v1->layer(), root.layer()->children()[0]);
  EXPECT_EQ(v2->layer(), root.layer()->children()[1]);
  EXPECT_EQ(v3->layer(), root.layer()->children()[2]);
}

TEST_F(ViewShadowTest, ShadowBoundsFollowView) {
  views::View view;
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewShadow shadow(&view, 1);

  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), shadow.shadow()->content_bounds());

  view.SetBoundsRect(gfx::Rect(100, 110, 120, 130));
  EXPECT_EQ(gfx::Rect(100, 110, 120, 130), shadow.shadow()->content_bounds());
}

TEST_F(ViewShadowTest, ShadowBoundsFollowIndirectViewBoundsChange) {
  views::View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(100, 100, 200, 200));

  views::View* parent = root.AddChildView(std::make_unique<views::View>());
  parent->SetBoundsRect(gfx::Rect(10, 20, 70, 80));
  views::View* view = parent->AddChildView(std::make_unique<views::View>());
  view->SetBoundsRect(gfx::Rect(5, 10, 20, 30));

  ViewShadow shadow(view, 1);
  EXPECT_EQ(gfx::Rect(15, 30, 20, 30), shadow.shadow()->content_bounds());

  parent->SetBoundsRect(gfx::Rect(5, 15, 60, 70));
  EXPECT_EQ(gfx::Rect(10, 25, 20, 30), shadow.shadow()->content_bounds());
}

TEST_F(ViewShadowTest, ShadowCornerRadius) {
  views::View view;
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  ViewShadow shadow(&view, 1);
  shadow.SetRoundedCornerRadius(5);

  EXPECT_EQ(gfx::RoundedCornersF(5), view.layer()->rounded_corner_radii());
  EXPECT_EQ(gfx::ShadowDetails::Get(1, 5).values,
            shadow.shadow()->details_for_testing()->values);

  shadow.SetRoundedCornerRadius(2);
  EXPECT_EQ(gfx::RoundedCornersF(2), view.layer()->rounded_corner_radii());
  EXPECT_EQ(gfx::ShadowDetails::Get(1, 2).values,
            shadow.shadow()->details_for_testing()->values);
}

TEST_F(ViewShadowTest, ViewDestruction) {
  views::View root;
  root.SetPaintToLayer();
  root.SetBoundsRect(gfx::Rect(10, 20, 30, 40));

  views::View* v1 = root.AddChildView(std::make_unique<views::View>());
  ViewShadow shadow(v1, 1);
  EXPECT_EQ(2u, root.layer()->children().size());

  delete v1;
  EXPECT_TRUE(root.layer()->children().empty());
}

TEST_F(ViewShadowTest, ShadowKeepsLayerType) {
  views::View view;
  view.SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  view.SetBoundsRect(gfx::Rect(10, 20, 30, 40));
  ViewShadow shadow(&view, 1);
  EXPECT_TRUE(view.layer());
  EXPECT_EQ(ui::LAYER_SOLID_COLOR, view.layer()->type());
}

}  // namespace ash
