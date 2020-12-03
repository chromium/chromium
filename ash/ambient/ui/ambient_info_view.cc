// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_info_view.h"

#include <memory>

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Appearance
constexpr int kMarginDip = 16;
constexpr int kSpacingDip = 8;

// Typography
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr int kDefaultFontSizeDip = 64;
constexpr int kDetailsFontSizeDip = 13;

}  // namespace

AmbientInfoView::AmbientInfoView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientInfoView);
  InitLayout();
}

AmbientInfoView::~AmbientInfoView() = default;

void AmbientInfoView::UpdateImageDetails(const base::string16& details) {
  details_label_->SetText(details);
}

void AmbientInfoView::SetTextTransform(const gfx::Transform& transform) {
  details_label_->layer()->SetTransform(transform);
  glanceable_info_view_->layer()->SetTransform(transform);
}

void AmbientInfoView::InitLayout() {
  gfx::Insets shadow_insets =
      gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues());

  // Full screen view with the glanceable info view and details label in the
  // lower left.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  layout->set_inside_border_insets(
      gfx::Insets(0, kMarginDip + shadow_insets.left(),
                  kMarginDip + shadow_insets.bottom(), 0));

  layout->set_between_child_spacing(kSpacingDip + shadow_insets.top() +
                                    shadow_insets.bottom());

  glanceable_info_view_ =
      AddChildView(std::make_unique<GlanceableInfoView>(delegate_));
  glanceable_info_view_->SetPaintToLayer();

  details_label_ = AddChildView(std::make_unique<views::Label>());
  details_label_->SetAutoColorReadabilityEnabled(false);
  details_label_->SetEnabledColor(kTextColor);
  details_label_->SetFontList(
      ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
          kDetailsFontSizeDip - kDefaultFontSizeDip));
  details_label_->SetShadows(ambient::util::GetTextShadowValues());
  details_label_->SetPaintToLayer();
  details_label_->layer()->SetFillsBoundsOpaquely(false);
}

BEGIN_METADATA(AmbientInfoView, views::View)
END_METADATA

}  // namespace ash
