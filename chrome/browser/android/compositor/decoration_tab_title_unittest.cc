// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/decoration_tab_title.h"

#include "cc/slim/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace android {
namespace {

TEST(DecorationTabTitleTest, BubbleShowHideReShowAndRepeatedSetBounds) {
  DecorationTabTitle title(
      /*resource_manager=*/nullptr,
      /*title_resource_id=*/0,
      /*icon_resource_id=*/0,
      /*spinner_resource_id=*/0,
      /*spinner_resource_id_incognito=*/0,
      /*fade_width=*/0,
      /*icon_start_padding=*/0,
      /*icon_end_padding=*/0,
      /*is_incognito=*/false,
      /*is_rtl=*/false,
      /*show_bubble=*/true,
      /*bubble_inner_dimension=*/6,
      /*bubble_outer_dimension=*/10,
      /*bubble_offset=*/2,
      /*bubble_inner_tint=*/0xFFFFFFFF,
      /*bubble_outer_tint=*/0xFF0000FF);
  title.SetShouldHideTitleText(true);

  title.setBounds(gfx::Size(100, 20));
  ASSERT_EQ(title.layer()->children().size(), 4u);
  cc::slim::Layer* bubble_layer = title.layer()->children()[3].get();
  EXPECT_FALSE(bubble_layer->hide_layer_and_subtree());

  // Append a sentinel child after the bubble layer so any unconditional
  // AddChild() re-attachment on repeated setBounds() calls would move it.
  scoped_refptr<cc::slim::Layer> sentinel = cc::slim::Layer::Create();
  title.layer()->AddChild(sentinel);

  title.setBounds(gfx::Size(100, 20));
  ASSERT_EQ(title.layer()->children().size(), 5u);
  EXPECT_EQ(title.layer()->children()[3].get(), bubble_layer);
  EXPECT_EQ(title.layer()->children()[4].get(), sentinel.get());
  EXPECT_FALSE(bubble_layer->hide_layer_and_subtree());

  title.SetShowBubble(false);
  title.setBounds(gfx::Size(100, 20));
  EXPECT_TRUE(bubble_layer->hide_layer_and_subtree());
  EXPECT_EQ(bubble_layer->parent(), title.layer().get());

  title.SetShowBubble(true);
  title.setBounds(gfx::Size(100, 20));
  EXPECT_FALSE(bubble_layer->hide_layer_and_subtree());
  EXPECT_EQ(title.layer()->children()[3].get(), bubble_layer);
  EXPECT_EQ(title.layer()->children()[4].get(), sentinel.get());
}

}  // namespace
}  // namespace android
