// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/ghost_image_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

namespace {

constexpr int kGhostCircleStrokeWidth = 2;
constexpr int kGhostColorOpacity = 0x4D;  // 30% opacity.
constexpr int kRootGridGhostColor = gfx::kGoogleGrey200;
constexpr int kInFolderGhostColor = gfx::kGoogleGrey700;
constexpr base::TimeDelta kGhostFadeInOutLength =
    base::TimeDelta::FromMilliseconds(180);
constexpr gfx::Tween::Type kGhostTween = gfx::Tween::FAST_OUT_SLOW_IN;
constexpr int kAlphaGradient = 2;
constexpr int kAlphaChannelFilter = 180;

// These values determine the thickness and appearance of the icon outlines.
constexpr float kBlurSizeOutline = 2.5;
constexpr float kBlurSizeThinOutline = 1;

// Amount of padding added on each side of the icon to avoid clipping when the
// ghost outline is generated.
constexpr int kGhostImagePadding = 5;

}  // namespace

GhostImageView::GhostImageView(bool is_folder, bool is_in_folder, int page)
    : is_hiding_(false),
      is_in_folder_(is_in_folder),
      is_folder_(is_folder),
      page_(page) {}

GhostImageView::~GhostImageView() {
  StopObservingImplicitAnimations();
}

void GhostImageView::Init(AppListItemView* drag_view,
                          const gfx::Rect& drop_target_bounds) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0f);
  SetBoundsRect(drop_target_bounds);
  icon_bounds_ = drag_view->GetIconBounds();

  if (is_folder_) {
    AppListFolderItem* folder_item =
        static_cast<AppListFolderItem*>(drag_view->item());
    num_items_ = std::min(FolderImage::kNumFolderTopItems,
                          folder_item->item_list()->item_count());

    // Create an outline for each item within the folder icon.
    for (size_t i = 0; i < num_items_.value(); i++) {
      gfx::ImageSkia inner_icon_outline =
          gfx::ImageSkiaOperations::CreateResizedImage(
              folder_item->item_list()->item_at(i)->GetIcon(
                  drag_view->GetAppListConfig().type()),
              skia::ImageOperations::RESIZE_BEST,
              drag_view->GetAppListConfig().item_icon_in_folder_icon_size());
      inner_folder_icon_outlines_.push_back(GetIconOutline(inner_icon_outline));
    }
  } else {
    // Create outline of app icon and set |outline_| to it.
    outline_ = GetIconOutline(drag_view->GetIconImage());
  }
}

void GhostImageView::FadeOut() {
  if (is_hiding_)
    return;
  is_hiding_ = true;
  DoAnimation(true /* fade out */);
}

void GhostImageView::FadeIn() {
  DoAnimation(false /* fade in */);
}

void GhostImageView::SetTransitionOffset(
    const gfx::Vector2d& transition_offset) {
  SetPosition(bounds().origin() + transition_offset);
}

const char* GhostImageView::GetClassName() const {
  return "GhostImageView";
}

void GhostImageView::DoAnimation(bool hide) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(kGhostFadeInOutLength);
  animation.SetTweenType(kGhostTween);

  if (hide) {
    animation.AddObserver(this);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.0f);
    return;
  }
  layer()->SetOpacity(1.0f);
}

void GhostImageView::OnPaint(gfx::Canvas* canvas) {
  const gfx::PointF circle_center(icon_bounds_.CenterPoint());

  // Draw a circle to represent the ghost image icon.
  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(is_in_folder_ ? kInFolderGhostColor
                                      : kRootGridGhostColor);
  circle_flags.setAlpha(kGhostColorOpacity);
  circle_flags.setStyle(cc::PaintFlags::kStroke_Style);
  circle_flags.setStrokeWidth(kGhostCircleStrokeWidth);

  if (is_folder_) {
    const float ghost_radius = icon_bounds_.width() / 2;

    // Draw circle to represent outline of folder.
    canvas->DrawCircle(circle_center, ghost_radius, circle_flags);

    // Draw a mask so inner folder icons do not overlap the outer circle.
    SkPath outer_circle_mask;
    outer_circle_mask.addCircle(circle_center.x(), circle_center.y(),
                                ghost_radius - kGhostCircleStrokeWidth / 2);
    canvas->ClipPath(outer_circle_mask, true);

    // Returns the bounds for each inner icon in the folder icon.
    std::vector<gfx::Rect> top_icon_bounds = FolderImage::GetTopIconsBounds(
        AppListConfig::instance(), icon_bounds_, num_items_.value());

    // Draw ghost items within the ghost folder circle.
    for (size_t i = 0; i < num_items_.value(); i++) {
      canvas->DrawImageInt(inner_folder_icon_outlines_[i],
                           top_icon_bounds[i].x() - kGhostImagePadding,
                           top_icon_bounds[i].y() - kGhostImagePadding);
    }
  } else {
    canvas->DrawImageInt(outline_, icon_bounds_.x() - kGhostImagePadding,
                         icon_bounds_.y() - kGhostImagePadding);
  }
  ImageView::OnPaint(canvas);
}

void GhostImageView::OnImplicitAnimationsCompleted() {
  // Delete this GhostImageView when the fade out animation is done.
  delete this;
}

// The implementation for GetIconOutline is copied and adapted from the Android
// Launcher. See com.android.launcher3.graphics.DragPreviewProvider.java in the
// Android source.

gfx::ImageSkia GhostImageView::GetIconOutline(
    const gfx::ImageSkia& original_icon) {
  gfx::ImageSkia icon_outline;

  original_icon.EnsureRepsForSupportedScales();
  for (gfx::ImageSkiaRep rep : original_icon.image_reps()) {
    // Only generate the outline for the ImageSkiaRep with the highest supported
    // scale.
    if (rep.scale() != original_icon.GetMaxSupportedScale())
      continue;

    SkBitmap bitmap(rep.GetBitmap());

    // Add padding to each side of the bitmap so the outline does not get cut
    // off by the edges from the original bitmap.
    gfx::Canvas padded_canvas(
        gfx::Size(bitmap.pixmap().width() + kGhostImagePadding * 2,
                  bitmap.pixmap().height() + kGhostImagePadding * 2),
        rep.scale(), false /* is_opaque */);
    padded_canvas.DrawImageInt(
        gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, rep.scale())),
        kGhostImagePadding, kGhostImagePadding);
    bitmap = padded_canvas.GetBitmap();

    const SkPixmap pixmap = bitmap.pixmap();
    const int width = pixmap.width();
    const int height = pixmap.height();

    SkBitmap preview;
    preview.allocN32Pixels(width, height);
    preview.eraseColor(SK_ColorTRANSPARENT);

    SkBitmap thick_outer_blur;
    SkBitmap bright_outline;
    SkBitmap thick_inner_blur;

    preview.setAlphaType(SkAlphaType::kUnpremul_SkAlphaType);
    thick_outer_blur.setAlphaType(SkAlphaType::kUnpremul_SkAlphaType);
    bright_outline.setAlphaType(SkAlphaType::kUnpremul_SkAlphaType);
    thick_inner_blur.setAlphaType(SkAlphaType::kUnpremul_SkAlphaType);

    // Remove most of the alpha channel so as to ignore shadows and other types
    // of partial transparency when defining the shape of the object.
    for (int x = 1; x < width; x++) {
      for (int y = 1; y < height; y++) {
        const SkColor* src_color =
            reinterpret_cast<SkColor*>(bitmap.getAddr32(0, y));
        SkColor* preview_color =
            reinterpret_cast<SkColor*>(preview.getAddr32(0, y));

        if (SkColorGetA(src_color[x]) < kAlphaChannelFilter) {
          preview_color[x] = SK_ColorTRANSPARENT;
        } else {
          preview_color[x] = SK_ColorWHITE;
        }
      }
    }

    SkPaint paint;
    paint.setAntiAlias(true);

    // Calculate the outer blur first.
    paint.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kOuter_SkBlurStyle,
                                               kBlurSizeOutline));
    SkIPoint outer_blur_offset;
    preview.extractAlpha(&thick_outer_blur, &paint, &outer_blur_offset);
    paint.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kOuter_SkBlurStyle,
                                               kBlurSizeThinOutline));
    SkIPoint bright_outline_offset;
    preview.extractAlpha(&bright_outline, &paint, &bright_outline_offset);

    // Calculate the inner blur.
    std::unique_ptr<SkCanvas> canvas = std::make_unique<SkCanvas>(preview);
    canvas->drawColor(SK_ColorBLACK, SkBlendMode::kSrcOut);
    paint.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kOuter_SkBlurStyle,
                                               kBlurSizeOutline));
    SkIPoint thick_inner_blur_offset;
    preview.extractAlpha(&thick_inner_blur, &paint, &thick_inner_blur_offset);

    // Mask out the inner blur.
    paint.setMaskFilter(nullptr);
    paint.setBlendMode(SkBlendMode::kDstOut);
    canvas = std::make_unique<SkCanvas>(thick_inner_blur);
    canvas->drawBitmap(preview, -thick_inner_blur_offset.fX,
                       -thick_inner_blur_offset.fY, &paint);
    canvas->drawRect(
        SkRect{0, 0, -thick_inner_blur_offset.fX, thick_inner_blur.height()},
        paint);
    canvas->drawRect(
        SkRect{0, 0, -thick_inner_blur.width(), thick_inner_blur_offset.fY},
        paint);

    // Draw the inner and outer blur.
    paint.setBlendMode(SkBlendMode::kPlus);
    canvas = std::make_unique<SkCanvas>(preview);
    canvas->drawColor(0, SkBlendMode::kClear);
    canvas->drawBitmap(thick_inner_blur, thick_inner_blur_offset.fX,
                       thick_inner_blur_offset.fY, &paint);
    canvas->drawBitmap(thick_outer_blur, outer_blur_offset.fX,
                       outer_blur_offset.fY, &paint);

    // Draw the bright outline.
    canvas->drawBitmap(bright_outline, bright_outline_offset.fX,
                       bright_outline_offset.fY, &paint);

    // Cleanup bitmaps.
    canvas.reset();
    bright_outline.reset();
    thick_outer_blur.reset();
    thick_inner_blur.reset();

    // Set the color and maximum allowed alpha for each pixel in |preview|.
    for (int x = 1; x < preview.width(); x++) {
      for (int y = 1; y < preview.height(); y++) {
        SkColor* current_color =
            reinterpret_cast<SkColor*>(preview.getAddr32(0, y));

        int current_alpha = SkColorGetA(current_color[x]);
        const int maximum_allowed_alpha = kGhostColorOpacity;

        if (current_alpha > maximum_allowed_alpha) {
          // Cap the current alpha at the maximum allowed alpha.
          current_alpha = maximum_allowed_alpha;
        } else if (current_alpha > 0 && current_alpha < maximum_allowed_alpha) {
          // To reduce blur on the edges of the outline, set the drop off of
          // alpha values below the |maximum_allowed_alpha| according to the
          // |kAlphaGradient|.
          const int new_alpha = (kAlphaGradient * current_alpha) -
                                (maximum_allowed_alpha * (kAlphaGradient - 1));
          current_alpha = std::max(0, new_alpha);
        }
        current_color[x] = SkColorSetA(SK_ColorWHITE, current_alpha);
      }
    }

    icon_outline.AddRepresentation(gfx::ImageSkiaRep(preview, rep.scale()));
  }

  const SkColor outline_color =
      is_in_folder_ ? kInFolderGhostColor : kRootGridGhostColor;
  return gfx::ImageSkiaOperations::CreateColorMask(icon_outline, outline_color);
}

}  // namespace ash
