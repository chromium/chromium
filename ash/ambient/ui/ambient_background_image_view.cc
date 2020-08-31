// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_background_image_view.h"

#include <memory>

#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
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
constexpr int kHorizontalMarginDip = 16;
constexpr int kVerticalMarginDip = 43;

// Typography.
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr int kDefaultFontSizeDip = 64;
constexpr int kDetailsFontSizeDip = 13;

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

  // Inits the attribution view. It also has a full-screen size and is
  // responsible for layout the details label at its bottom left corner.
  views::View* attribution_view = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* attribution_layout =
      attribution_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  attribution_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  attribution_layout->set_inside_border_insets(
      gfx::Insets(0, kHorizontalMarginDip, kVerticalMarginDip, 0));

  // Inits the details label.
  details_label_ =
      attribution_view->AddChildView(std::make_unique<views::Label>());
  details_label_->SetAutoColorReadabilityEnabled(false);
  details_label_->SetEnabledColor(kTextColor);
  details_label_->SetFontList(
      ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
          kDetailsFontSizeDip - kDefaultFontSizeDip));
  details_label_->SetShadows(ambient::util::GetTextShadowValues());
  details_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  details_label_->SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_BOTTOM);
}

BEGIN_METADATA(AmbientBackgroundImageView, views::ImageView)
END_METADATA

}  // namespace ash
