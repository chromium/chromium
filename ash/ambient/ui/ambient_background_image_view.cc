// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_background_image_view.h"

#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

gfx::ImageSkia ResizeImage(const gfx::ImageSkia& image,
                           const gfx::Size& view_size,
                           const bool force_resize_to_fit) {
  if (image.isNull())
    return gfx::ImageSkia();

  const double image_width = image.width();
  const double image_height = image.height();
  const double view_width = view_size.width();
  const double view_height = view_size.height();
  const double horizontal_ratio = view_width / image_width;
  const double vertical_ratio = view_height / image_height;
  const double image_ratio = image_height / image_width;
  const double view_ratio = view_height / view_width;

  double scale = 1.0;

  // If force fitting is enabled, we will always scale to the smaller ratio to
  // ensure that no part of the image is cropped out and the whole image is
  // shown on the screen with possible black bars.
  if (force_resize_to_fit) {
    scale = std::min(horizontal_ratio, vertical_ratio);
  } else {
    // If the image and the container view has the same orientation, e.g. both
    // portrait, the |scale| will make the image filled the whole view with
    // possible cropping on one direction. If they are in different orientation,
    // the |scale| will display the image in the view without any cropping, but
    // with empty background.
    scale = (image_ratio - 1) * (view_ratio - 1) > 0
                ? std::max(horizontal_ratio, vertical_ratio)
                : std::min(horizontal_ratio, vertical_ratio);
  }
  const gfx::Size& resized = gfx::ScaleToCeiledSize(image.size(), scale);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, resized);
}

gfx::ImageSkia MaybeRotateImage(const gfx::ImageSkia& image,
                                const gfx::Size& view_size,
                                views::Widget* widget) {
  if (image.isNull())
    return image;

  const double image_width = image.width();
  const double image_height = image.height();
  const double view_width = view_size.width();
  const double view_height = view_size.height();
  const double image_ratio = image_height / image_width;
  const double view_ratio = view_height / view_width;

  // Rotate the image to have the same orientation as the display.
  // Keep the relative orientation between the image and the display in portrait
  // mode.
  if ((image_ratio - 1) * (view_ratio - 1) < 0) {
    bool should_rotate = false;
    SkBitmapOperations::RotationAmount rotation_amount;
    const int64_t display_id =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(widget->GetNativeWindow())
            .id();
    const auto active_rotation = Shell::Get()
                                     ->display_manager()
                                     ->GetDisplayInfo(display_id)
                                     .GetActiveRotation();
    switch (active_rotation) {
      case display::Display::ROTATE_90:
        should_rotate = true;
        rotation_amount = SkBitmapOperations::RotationAmount::ROTATION_270_CW;
        break;
      case display::Display::ROTATE_270:
        should_rotate = true;
        rotation_amount = SkBitmapOperations::RotationAmount::ROTATION_90_CW;
        break;
      default:
        // No action.
        break;
    }
    if (should_rotate) {
      return gfx::ImageSkiaOperations::CreateRotatedImage(image,
                                                          rotation_amount);
    }
  }

  return image;
}

}  // namespace

AmbientBackgroundImageView::AmbientBackgroundImageView(
    AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientBackgroundImageView);
  InitLayout();
}

AmbientBackgroundImageView::~AmbientBackgroundImageView() = default;

void AmbientBackgroundImageView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (!GetVisible())
    return;

  if (width() == 0)
    return;

  UpdateLayout();

  // When bounds changes, recalculate the visibility of related image view.
  UpdateRelatedImageViewVisibility();
  UpdateImageDetails(details_, related_details_);
}

void AmbientBackgroundImageView::OnViewBoundsChanged(
    views::View* observed_view) {
  if (observed_view == image_view_)
    SetResizedImage(image_view_, image_unscaled_);
  else
    SetResizedImage(related_image_view_, related_image_unscaled_);
}

void AmbientBackgroundImageView::UpdateImage(
    const gfx::ImageSkia& image,
    const gfx::ImageSkia& related_image,
    bool is_portrait,
    ::ambient::TopicType type) {
  image_unscaled_ = image;
  related_image_unscaled_ = related_image;
  is_portrait_ = is_portrait;
  topic_type_ = type;

  ambient_peripheral_ui_->UpdateGlanceableInfoPosition();

  const bool has_change = UpdateRelatedImageViewVisibility();

  // If there is no change in the visibility of related image view, call
  // SetResizedImages() directly. Otherwise it will be called from
  // OnViewBoundsChanged().
  if (!has_change) {
    SetResizedImage(image_view_, image_unscaled_);
    SetResizedImage(related_image_view_, related_image_unscaled_);
  }
}

void AmbientBackgroundImageView::UpdateImageDetails(
    const std::u16string& details,
    const std::u16string& related_details) {
  details_ = details;
  related_details_ = related_details;
  ambient_peripheral_ui_->UpdateImageDetails(
      details, MustShowPairs() ? related_details : std::u16string());
}

gfx::ImageSkia AmbientBackgroundImageView::GetCurrentImage() {
  return image_view_->GetImage();
}

gfx::Rect AmbientBackgroundImageView::GetImageBoundsInScreenForTesting() const {
  gfx::Rect rect = image_view_->GetImageBounds();
  views::View::ConvertRectToScreen(image_view_, &rect);
  return rect;
}

gfx::Rect AmbientBackgroundImageView::GetRelatedImageBoundsInScreenForTesting()
    const {
  if (!related_image_view_->GetVisible())
    return gfx::Rect();

  gfx::Rect rect = related_image_view_->GetImageBounds();
  views::View::ConvertRectToScreen(related_image_view_, &rect);
  return rect;
}

void AmbientBackgroundImageView::ResetRelatedImageForTesting() {
  related_image_unscaled_ = gfx::ImageSkia();
  UpdateRelatedImageViewVisibility();
}

void AmbientBackgroundImageView::InitLayout() {
  static const views::FlexSpecification kUnboundedScaleToZero(
      views::MinimumFlexSizeRule::kScaleToZero,
      views::MaximumFlexSizeRule::kUnbounded);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Inits container for images.
  image_container_ = AddChildView(std::make_unique<views::View>());
  image_layout_ =
      image_container_->SetLayoutManager(std::make_unique<views::FlexLayout>());

  image_view_ =
      image_container_->AddChildView(std::make_unique<views::ImageView>());
  // Set a place holder size for Flex layout to assign bounds.
  image_view_->SetPreferredSize(gfx::Size(1, 1));
  image_view_->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZero);
  observed_views_.AddObservation(image_view_.get());

  related_image_view_ =
      image_container_->AddChildView(std::make_unique<views::ImageView>());
  // Set a place holder size for Flex layout to assign bounds.
  related_image_view_->SetPreferredSize(gfx::Size(1, 1));
  related_image_view_->SetProperty(views::kFlexBehaviorKey,
                                   kUnboundedScaleToZero);
  observed_views_.AddObservation(related_image_view_.get());

  ambient_peripheral_ui_ =
      AddChildView(std::make_unique<AmbientSlideshowPeripheralUi>(delegate_));
}

void AmbientBackgroundImageView::UpdateLayout() {
  if (width() > height()) {
    image_layout_->SetOrientation(views::LayoutOrientation::kHorizontal);

    // Set spacing between two images.
    related_image_view_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, kMarginLeftOfRelatedImageDip, 0, 0));
  } else {
    image_layout_->SetOrientation(views::LayoutOrientation::kVertical);

    // Set spacing between two images.
    related_image_view_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(kMarginLeftOfRelatedImageDip, 0, 0, 0));
  }

  image_layout_->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  image_layout_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

bool AmbientBackgroundImageView::UpdateRelatedImageViewVisibility() {
  const bool did_show_pair = related_image_view_->GetVisible();
  const bool show_pair = MustShowPairs() && HasPairedImages();
  related_image_view_->SetVisible(show_pair);
  return did_show_pair != show_pair;
}

void AmbientBackgroundImageView::SetResizedImage(
    views::ImageView* image_view,
    const gfx::ImageSkia& image_unscaled) {
  if (!image_view->GetVisible())
    return;

  if (image_unscaled.isNull())
    return;

  gfx::ImageSkia image_rotated =
      topic_type_ == ::ambient::TopicType::kGeo
          ? MaybeRotateImage(image_unscaled, image_view->size(), GetWidget())
          : image_unscaled;
  image_view->SetImage(
      ResizeImage(image_rotated, image_view->size(), force_resize_to_fit_));

  // Intend to update the image origin in image view.
  // There is no bounds change or preferred size change when updating image from
  // landscape to portrait when device is in portrait orientation because we
  // only show one photo. Call ResetImageSize() to trigger UpdateImageOrigin().
  image_view->ResetImageSize();
}

void AmbientBackgroundImageView::SetPeripheralUiVisibility(bool visible) {
  ambient_peripheral_ui_->SetVisible(visible);
}

void AmbientBackgroundImageView::SetForceResizeToFit(bool force_resize_to_fit) {
  force_resize_to_fit_ = force_resize_to_fit;
}

bool AmbientBackgroundImageView::MustShowPairs() const {
  const bool landscape_mode_portrait_image = width() > height() && is_portrait_;
  const bool portrait_mode_landscape_image =
      width() < height() && !is_portrait_;
  return landscape_mode_portrait_image || portrait_mode_landscape_image;
}

bool AmbientBackgroundImageView::HasPairedImages() const {
  return !image_unscaled_.isNull() && !related_image_unscaled_.isNull();
}

BEGIN_METADATA(AmbientBackgroundImageView)
END_METADATA

}  // namespace ash
