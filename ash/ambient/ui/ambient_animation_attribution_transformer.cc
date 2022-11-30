// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_attribution_transformer.h"

#include <string>
#include <utility>

#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/logging.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/paint/skottie_transform_property_value.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"

namespace ash {
namespace {

// Amount of padding there should be from the bottom-right of the
// AnimatedImageView to the bottom-right of the attribution text box.
constexpr gfx::Vector2d kTextBoxPaddingDip = gfx::Vector2d(24, 24);

}  // namespace

// This translates between 2 coordinate systems. The first coordinate system
// (the one translating from) is the views coordinate system where the origin
// is the top-left of the view. In practice, the typical case looks like this:
//
// Animation
// +-----------------------------------------------+
// |                                               |
// |(0, 0) View                                    |
// +-----------------------------------------------|
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |                                               |
// |-------------------------------------------+   |
// |                           Attribution Text|   |
// |-------------------------------------------+   |
// |                                               |
// +-----------------------------------------------+
// |                                               |
// |                                               |
// +-----------------------------------------------+
//
// Note in the above, the animation's width matches the view's width, and its
// height exceeds that of the view (the upper and lower parts of the animation
// get cropped out). The origin is the top-left of the view, so the top-left of
// the animation has coordinates (0, <some negative number>). The attribution
// text box's bottom right corner has coordinates
// (view_width - 24, view_height - 24).
//
// The second set of coordinates (the one translating to) is the original
// animation's coordinate system. "Original" here refers to the
// coordinates baked into the Lottie file. Visually, it looks the same as the
// picture above, except:
// * The origin is the top-left of the animation.
// * The animation's width/height are those of the original animation (baked
//   into the Lottie file), as opposed to those of the "scaled" animation that
//   was scaled to reflect the view's bounds/dimensions.
//
// Note that although the typical case is illustrated above, the implementation
// was written generically to account for all cases.
void AmbientAnimationAttributionTransformer::TransformTextBox(
    views::AnimatedImageView& animated_image_view) {
  gfx::Transform view_to_animation_transform;
  // 1) Change the origin from the top-left of the view to the top-left of the
  //    scaled animation.
  DCHECK(!animated_image_view.GetImageBounds().IsEmpty());
  gfx::Vector2d scaled_animation_origin_offset =
      animated_image_view.GetImageBounds().origin().OffsetFromOrigin();
  view_to_animation_transform.Translate(-scaled_animation_origin_offset);
  // 2) Reset the coordinates from the "scaled" animation dimensions (scaled to
  //    fit the view) to the original animation dimensions baked into the Lottie
  //    file.
  lottie::Animation* animation = animated_image_view.animated_image();
  DCHECK(animation);
  gfx::Size original_animation_size = animation->GetOriginalSize();
  gfx::Size scaled_animation_size = animated_image_view.GetImageBounds().size();
  view_to_animation_transform.PostScale(
      static_cast<float>(original_animation_size.width()) /
          scaled_animation_size.width(),
      static_cast<float>(original_animation_size.height()) /
          scaled_animation_size.height());

  // Apply transformation to the bottom-right corner of the text box. The
  // bottom-right corner is arbitrary here and is just used as a point of
  // reference when building the final transformed text box's coordinates.
  gfx::Rect view_bounds = animated_image_view.GetContentsBounds();
  DCHECK(!view_bounds.IsEmpty())
      << "AnimatedImageView's content bounds must be initialized before "
         "transforming the text box.";
  gfx::Point text_box_bottom_right = view_to_animation_transform.MapPoint(
      view_bounds.bottom_right() - kTextBoxPaddingDip);
  // In the majority of cases, the bottom-right of the text box will already be
  // within the boundaries of the original animation. There are some corner
  // cases though (ex: fitting a landscape animation file to portrait view)
  // where the bottom-right will be outside the animation's boundaries. In these
  // cases, clamp the text box's coordinates to the bottom-right of the
  // animation, or the text box will ultimately not be rendered.
  text_box_bottom_right.SetToMin(
      gfx::Rect(original_animation_size).bottom_right());

  for (const std::string& text_node_name :
       animation->skottie()->GetTextNodeNames()) {
    if (!IsCustomizableLottieId(text_node_name)) {
      DVLOG(4) << "Ignoring non-attribution text node";
      continue;
    }

    cc::SkottieResourceIdHash attribution_node_id =
        cc::HashSkottieResourceId(text_node_name);

    DCHECK(animation->text_map().contains(attribution_node_id));
    cc::SkottieTextPropertyValue& attribution_val =
        animation->text_map().at(attribution_node_id);
    // Text box's height stays the same as what's specified in the lottie file.
    gfx::RectF new_text_box = attribution_val.box();
    new_text_box.set_width(text_box_bottom_right.x());
    new_text_box.set_origin(
        gfx::PointF(0, text_box_bottom_right.y() - new_text_box.height()));

    // One final transform: The text box's coordinates must be relative to the
    // text attribution layer's "position" in the animation file (an arbitrary
    // point specified in Adobe After-Effects when the animation is built). It
    // is effectively the "local origin" for the text box, and may be different
    // for each attribution node in the animation.
    DCHECK(animation->skottie()->GetCurrentTransformPropertyValues().contains(
        attribution_node_id));
    gfx::Transform attribution_layer_shift;
    attribution_layer_shift.Translate(-animation->skottie()
                                           ->GetCurrentTransformPropertyValues()
                                           .at(attribution_node_id)
                                           .position.OffsetFromOrigin());
    attribution_val.set_box(attribution_layer_shift.MapRect(new_text_box));
  }
}

}  // namespace ash
