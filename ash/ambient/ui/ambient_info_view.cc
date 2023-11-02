// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_info_view.h"

#include <memory>

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Appearance
constexpr int kMarginDip = 16;
constexpr int kSpacingDip = 8;

// Typography
constexpr int kDefaultFontSizeDip = 64;
constexpr int kDetailsFontSizeDip = 13;
constexpr int kTimeFontSizeDip = 64;

views::Label* AddLabel(views::View* parent) {
  auto* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(ambient::util::GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  label->SetFontList(ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
      kDetailsFontSizeDip - kDefaultFontSizeDip));
  label->SetPaintToLayer();
  label->layer()->SetFillsBoundsOpaquely(false);

  return label;
}
}  // namespace

AmbientInfoView::AmbientInfoView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientInfoView);
  InitLayout();
}

AmbientInfoView::~AmbientInfoView() = default;

void AmbientInfoView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  details_label_->SetShadows(
      ambient::util::GetTextShadowValues(color_provider));
  related_details_label_->SetShadows(
      ambient::util::GetTextShadowValues(color_provider));
}

void AmbientInfoView::UpdateImageDetails(
    const std::u16string& details,
    const std::u16string& related_details) {
  details_label_->SetText(details);
  related_details_label_->SetText(related_details);
  related_details_label_->SetVisible(!related_details.empty() &&
                                     details != related_details);
}

void AmbientInfoView::SetTextTransform(const gfx::Transform& transform) {
  details_label_->layer()->SetTransform(transform);
  related_details_label_->layer()->SetTransform(transform);
  glanceable_info_view_->layer()->SetTransform(transform);
}

void AmbientInfoView::InitLayout() {
  gfx::Insets shadow_insets =
      gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues(nullptr));

  // Full screen view with the glanceable info view and details label in the
  // lower left.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  layout->set_inside_border_insets(
      gfx::Insets::TLBR(0, kMarginDip + shadow_insets.left(),
                        kMarginDip + shadow_insets.bottom(), 0));

  layout->set_between_child_spacing(kSpacingDip + shadow_insets.top() +
                                    shadow_insets.bottom());

  glanceable_info_view_ = AddChildView(std::make_unique<GlanceableInfoView>(
      delegate_, kTimeFontSizeDip, /*time_temperature_font_color=*/
      ambient::util::GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary)));
  glanceable_info_view_->SetPaintToLayer();

  details_label_ = AddLabel(this);
  related_details_label_ = AddLabel(this);
}

BEGIN_METADATA(AmbientInfoView, views::View)
END_METADATA

}  // namespace ash
