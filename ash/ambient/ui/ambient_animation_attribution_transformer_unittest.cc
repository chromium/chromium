// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_attribution_transformer.h"

#include <memory>
#include <string>
#include <string_view>

#include "ash/ambient/test/ambient_test_util.h"
#include "base/check.h"

#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_transform_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {
namespace {

using ::testing::Eq;

constexpr float kTextBoxCoordinatesTolerance = 0.5f;

class AmbientAnimationAttributionTransformerTest : public ::testing::Test {
 protected:
  AmbientAnimationAttributionTransformerTest()
      : AmbientAnimationAttributionTransformerTest(
            GenerateLottieCustomizableIdForTesting(/*unique_id=*/1),
            GenerateLottieCustomizableIdForTesting(/*unique_id=*/2)) {}

  AmbientAnimationAttributionTransformerTest(std::string_view text_node_1_name,
                                             std::string_view text_node_2_name)
      : text_node_1_name_(text_node_1_name),
        text_node_2_name_(text_node_2_name) {
    view_.SetAnimatedImage(std::make_unique<lottie::Animation>(
        cc::CreateSkottieFromString(cc::CreateCustomLottieDataWith2TextNodes(
            text_node_1_name_, text_node_2_name_))));
    view_.SetHorizontalAlignment(views::ImageViewBase::Alignment::kCenter);
    view_.SetVerticalAlignment(views::ImageViewBase::Alignment::kCenter);
    animation_original_size_ = view_.animated_image()->GetOriginalSize();
  }

  gfx::RectF GetAbsoluteTextBoxCoordinates(std::string_view text_node_name) {
    cc::SkottieResourceIdHash text_node_id =
        cc::HashSkottieResourceId(text_node_name);
    cc::SkottieTransformPropertyValueMap transform_properties =
        view_.animated_image()->skottie()->GetCurrentTransformPropertyValues();
    CHECK(transform_properties.contains(text_node_id))
        << "Transform property not found for " << text_node_name;
    const cc::SkottieTextPropertyValueMap& text_map =
        view_.animated_image()->text_map();
    CHECK(text_map.contains(text_node_id))
        << "Text property not found for " << text_node_name;
    return text_map.at(text_node_id).box() +
           transform_properties.at(text_node_id).position.OffsetFromOrigin();
  }

  const std::string text_node_1_name_;
  const std::string text_node_2_name_;
  views::AnimatedImageView view_;
  gfx::Size animation_original_size_;
};

class AmbientAnimationAttributionTransformerTestWithStaticText
    : public AmbientAnimationAttributionTransformerTest {
 protected:
  AmbientAnimationAttributionTransformerTestWithStaticText()
      : AmbientAnimationAttributionTransformerTest(
            cc::kLottieDataWith2TextNode1,
            cc::kLottieDataWith2TextNode2) {}
};

TEST_F(AmbientAnimationAttributionTransformerTest, ViewSmallerThanAnimation) {
  const gfx::Rect view_bounds(animation_original_size_.width() / 2,
                              animation_original_size_.height() / 2);
  view_.SetBoundsRect(view_bounds);
  AmbientAnimationAttributionTransformer::TransformTextBox(view_);

  gfx::RectF expected_box;
  expected_box.set_width((animation_original_size_.width() / 4) +
                         (view_bounds.width() - 24));
  expected_box.set_height(cc::kLottieDataWith2TextNode1Box.height());
  expected_box.set_x(0);
  expected_box.set_y((animation_original_size_.height() / 4) +
                     (view_bounds.height() - 24 - expected_box.height()));
  EXPECT_RECTF_NEAR(GetAbsoluteTextBoxCoordinates(text_node_1_name_),
                    expected_box, kTextBoxCoordinatesTolerance);

  expected_box.set_height(cc::kLottieDataWith2TextNode2Box.height());
  expected_box.set_y((animation_original_size_.height() / 4) +
                     (view_bounds.height() - 24 - expected_box.height()));
  EXPECT_RECTF_NEAR(GetAbsoluteTextBoxCoordinates(text_node_2_name_),
                    expected_box, kTextBoxCoordinatesTolerance);
}

TEST_F(AmbientAnimationAttributionTransformerTest, ViewLargerThanAnimation) {
  const gfx::Rect view_bounds(animation_original_size_.width() * 2,
                              animation_original_size_.height() * 2);
  view_.SetBoundsRect(view_bounds);
  AmbientAnimationAttributionTransformer::TransformTextBox(view_);
  gfx::RectF expected_box;
  expected_box.set_width(animation_original_size_.width());
  expected_box.set_height(cc::kLottieDataWith2TextNode1Box.height());
  expected_box.set_x(0);
  expected_box.set_y(animation_original_size_.height() - expected_box.height());
  EXPECT_RECTF_NEAR(GetAbsoluteTextBoxCoordinates(text_node_1_name_),
                    expected_box, kTextBoxCoordinatesTolerance);
}

TEST_F(AmbientAnimationAttributionTransformerTestWithStaticText,
       ViewLargerThanAnimation) {
  view_.SetBoundsRect(gfx::Rect(animation_original_size_));
  AmbientAnimationAttributionTransformer::TransformTextBox(view_);
  // Static text nodes shouldn't change.
  EXPECT_RECTF_EQ(view_.animated_image()
                      ->text_map()
                      .at(cc::HashSkottieResourceId(text_node_1_name_))
                      .box(),
                  cc::kLottieDataWith2TextNode1Box);
  EXPECT_RECTF_EQ(view_.animated_image()
                      ->text_map()
                      .at(cc::HashSkottieResourceId(text_node_2_name_))
                      .box(),
                  cc::kLottieDataWith2TextNode2Box);
}

}  // namespace
}  // namespace ash
