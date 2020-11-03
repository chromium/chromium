// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_background_image_view.h"

#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "base/rand_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginDip = 16;
constexpr int kSpacingDip = 8;
constexpr int kMediaStringMarginDip = 32;

// Typography.
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr int kDefaultFontSizeDip = 64;
constexpr int kDetailsFontSizeDip = 13;

// The dicretion to translate glanceable info views in the x/y coordinates.  `1`
// means positive translate, `-1` negative.
int translate_x_direction = 1;
int translate_y_direction = -1;
// The current x/y translation of glanceable info views in Dip.
int current_x_translation = 0;
int current_y_translation = 0;

gfx::ImageSkia ResizeImage(const gfx::ImageSkia& image,
                           const gfx::Size& view_size) {
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

  // If the image and the container view has the same orientation, e.g. both
  // portrait, the |scale| will make the image filled the whole view with
  // possible cropping on one direction. If they are in different orientation,
  // the |scale| will display the image in the view without any cropping, but
  // with empty background.
  const double scale = (image_ratio - 1) * (view_ratio - 1) > 0
                           ? std::max(horizontal_ratio, vertical_ratio)
                           : std::min(horizontal_ratio, vertical_ratio);
  const gfx::Size& resized = gfx::ScaleToCeiledSize(image.size(), scale);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, resized);
}

}  // namespace

AmbientBackgroundImageView::AmbientBackgroundImageView(
    AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AssistantViewID::kAmbientBackgroundImageView);
  InitLayout();
}

AmbientBackgroundImageView::~AmbientBackgroundImageView() = default;

void AmbientBackgroundImageView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  if (!GetVisible())
    return;

  if (width() == 0)
    return;

  // When bounds changes, recalculate the visibility of related image view.
  UpdateRelatedImageViewVisibility();
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
    const gfx::ImageSkia& related_image) {
  image_unscaled_ = image;
  related_image_unscaled_ = related_image;

  UpdateGlanceableInfoPosition();

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
    const base::string16& details) {
  details_label_->SetText(details);
}

const gfx::ImageSkia& AmbientBackgroundImageView::GetCurrentImage() {
  return image_view_->GetImage();
}

gfx::Rect AmbientBackgroundImageView::GetImageBoundsForTesting() const {
  return image_view_->GetImageBounds();
}

gfx::Rect AmbientBackgroundImageView::GetRelatedImageBoundsForTesting() const {
  return related_image_view_->GetVisible()
             ? related_image_view_->GetImageBounds()
             : gfx::Rect();
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
  views::FlexLayout* image_layout =
      image_container_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  image_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  image_layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  image_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  image_view_ =
      image_container_->AddChildView(std::make_unique<views::ImageView>());
  // Set a place holder size for Flex layout to assign bounds.
  image_view_->SetPreferredSize(gfx::Size(1, 1));
  image_view_->SetProperty(views::kFlexBehaviorKey, kUnboundedScaleToZero);
  observed_views_.Add(image_view_);

  related_image_view_ =
      image_container_->AddChildView(std::make_unique<views::ImageView>());
  // Set a place holder size for Flex layout to assign bounds.
  related_image_view_->SetPreferredSize(gfx::Size(1, 1));
  related_image_view_->SetProperty(views::kFlexBehaviorKey,
                                   kUnboundedScaleToZero);
  observed_views_.Add(related_image_view_);

  // Set spacing between two images.
  related_image_view_->SetProperty(
      views::kMarginsKey, gfx::Insets(0, kMarginLeftOfRelatedImageDip, 0, 0));

  gfx::Insets shadow_insets =
      gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues());

  // Inits the attribution view. It also has a full-screen size and is
  // responsible for layout the details label at its bottom left corner.
  views::View* attribution_view = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* attribution_layout =
      attribution_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  attribution_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  attribution_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  attribution_layout->set_inside_border_insets(
      gfx::Insets(0, kMarginDip + shadow_insets.left(),
                  kMarginDip + shadow_insets.bottom(), 0));

  attribution_layout->set_between_child_spacing(
      kSpacingDip + shadow_insets.top() + shadow_insets.bottom());

  glanceable_info_view_ = attribution_view->AddChildView(
      std::make_unique<GlanceableInfoView>(delegate_));
  glanceable_info_view_->SetPaintToLayer();

  // Inits the details label.
  details_label_ =
      attribution_view->AddChildView(std::make_unique<views::Label>());
  details_label_->SetAutoColorReadabilityEnabled(false);
  details_label_->SetEnabledColor(kTextColor);
  details_label_->SetFontList(
      ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
          kDetailsFontSizeDip - kDefaultFontSizeDip));
  details_label_->SetShadows(ambient::util::GetTextShadowValues());
  details_label_->SetPaintToLayer();
  details_label_->layer()->SetFillsBoundsOpaquely(false);

  // Inits the media string view. The media string view is positioned on the
  // right-top corner of the container.
  views::View* media_string_view_container_ =
      AddChildView(std::make_unique<views::View>());
  views::BoxLayout* media_string_layout =
      media_string_view_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical));
  media_string_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  media_string_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  media_string_layout->set_inside_border_insets(
      gfx::Insets(kMediaStringMarginDip + shadow_insets.top(), 0, 0,
                  kMediaStringMarginDip + shadow_insets.right()));
  media_string_view_ = media_string_view_container_->AddChildView(
      std::make_unique<MediaStringView>());
  media_string_view_->SetVisible(false);
}

void AmbientBackgroundImageView::UpdateGlanceableInfoPosition() {
  constexpr int kStepDP = 5;
  constexpr int kMaxTranslationDip = 20;

  // Move the translation point randomly one step on each x/y direction.
  int x_increment = kStepDP * base::RandInt(0, 1);
  int y_increment = x_increment == 0 ? kStepDP : kStepDP * base::RandInt(0, 1);
  current_x_translation += translate_x_direction * x_increment;
  current_y_translation += translate_y_direction * y_increment;

  // If the translation point is out of bounds, reset it within bounds and
  // reverse the direction.
  if (current_x_translation < 0) {
    translate_x_direction = 1;
    current_x_translation = 0;
  } else if (current_x_translation > kMaxTranslationDip) {
    translate_x_direction = -1;
    current_x_translation = kMaxTranslationDip;
  }

  if (current_y_translation > 0) {
    translate_y_direction = -1;
    current_y_translation = 0;
  } else if (current_y_translation < -kMaxTranslationDip) {
    translate_y_direction = 1;
    current_y_translation = -kMaxTranslationDip;
  }

  gfx::Transform transform;
  transform.Translate(current_x_translation, current_y_translation);
  glanceable_info_view_->layer()->SetTransform(transform);
  details_label_->layer()->SetTransform(transform);

  if (media_string_view_->GetVisible()) {
    gfx::Transform media_string_transform;
    media_string_transform.Translate(-current_x_translation,
                                     -current_y_translation);
    media_string_view_->layer()->SetTransform(media_string_transform);
  }
}

bool AmbientBackgroundImageView::UpdateRelatedImageViewVisibility() {
  const bool did_show_pair = related_image_view_->GetVisible();
  const bool show_pair = IsLandscapeOrientation() && HasPairedImages();
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

  image_view->SetImage(ResizeImage(image_unscaled, image_view->size()));

  // Intend to update the image origin in image view.
  // There is no bounds change or preferred size change when updating image from
  // landscape to portrait when device is in portrait orientation because we
  // only show one photo. Call ResetImageSize() to trigger UpdateImageOrigin().
  image_view->ResetImageSize();
}

bool AmbientBackgroundImageView::IsLandscapeOrientation() const {
  return width() > height();
}

bool AmbientBackgroundImageView::HasPairedImages() const {
  return !image_unscaled_.isNull() && !related_image_unscaled_.isNull();
}

BEGIN_METADATA(AmbientBackgroundImageView, views::View)
END_METADATA

}  // namespace ash
