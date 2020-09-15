// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_background_image_view.h"

#include <memory>

#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "base/rand_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMarginDip = 16;
constexpr int kSpacingDip = 8;

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

}  // namespace

AmbientBackgroundImageView::AmbientBackgroundImageView(
    AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AssistantViewID::kAmbientBackgroundImageView);
  InitLayout();
}

AmbientBackgroundImageView::~AmbientBackgroundImageView() = default;

// views::View:
bool AmbientBackgroundImageView::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnBackgroundPhotoEvents();
  return true;
}

// views::View:
void AmbientBackgroundImageView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    delegate_->OnBackgroundPhotoEvents();
    event->SetHandled();
  }
}

void AmbientBackgroundImageView::UpdateImage(const gfx::ImageSkia& img) {
  image_view_->SetImage(img);

  UpdateGlanceableInfoPosition();
}

void AmbientBackgroundImageView::UpdateImageDetails(
    const base::string16& details) {
  details_label_->SetText(details);
}

const gfx::ImageSkia& AmbientBackgroundImageView::GetCurrentImage() {
  return image_view_->GetImage();
}

gfx::Rect AmbientBackgroundImageView::GetCurrentImageBoundsForTesting() const {
  return image_view_->GetImageBounds();
}

void AmbientBackgroundImageView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Inits the image view. This view should have the same size of the screen.
  image_view_ = AddChildView(std::make_unique<views::ImageView>());

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
}

BEGIN_METADATA(AmbientBackgroundImageView, views::View)
END_METADATA

}  // namespace ash
