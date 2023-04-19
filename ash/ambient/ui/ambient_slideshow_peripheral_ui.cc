// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"

#include <memory>

#include "ash/ambient/ui/ambient_shield_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/jitter_calculator.h"
#include "ash/ambient/ui/media_string_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/style/ash_color_id.h"
#include "base/logging.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMediaStringMarginDip = 32;

constexpr JitterCalculator::Config kDefaultGlanceableInfoJitterConfig = {
    /*step_size=*/5,
    /*x_min_translation=*/0,
    /*x_max_translation=*/20,
    /*y_min_translation=*/-20,
    /*y_max_translation=*/0};

}  // namespace

std::unique_ptr<JitterCalculator>
AmbientSlideshowPeripheralUi::CreateDefaultJitterCalculator() {
  return std::make_unique<JitterCalculator>(kDefaultGlanceableInfoJitterConfig);
}

AmbientSlideshowPeripheralUi::AmbientSlideshowPeripheralUi(
    AmbientViewDelegate* delegate,
    JitterCalculator* jitter_calculator)
    : owned_jitter_calculator_(
          jitter_calculator ? nullptr : CreateDefaultJitterCalculator()),
      jitter_calculator_(jitter_calculator ? jitter_calculator
                                           : owned_jitter_calculator_.get()) {
  CHECK(delegate);
  CHECK(jitter_calculator_);
  SetID(AmbientViewID::kAmbientSlideshowPeripheralUi);
  InitLayout(delegate);
}

AmbientSlideshowPeripheralUi::~AmbientSlideshowPeripheralUi() = default;

void AmbientSlideshowPeripheralUi::InitLayout(AmbientViewDelegate* delegate) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(std::make_unique<AmbientShieldView>());

  ambient_info_view_ =
      AddChildView(std::make_unique<AmbientInfoView>(delegate));

  gfx::Insets shadow_insets =
      gfx::ShadowValue::GetMargin(ambient::util::GetTextShadowValues(nullptr));

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
      gfx::Insets::TLBR(kMediaStringMarginDip + shadow_insets.top(), 0, 0,
                        kMediaStringMarginDip + shadow_insets.right()));
  media_string_view_ = media_string_view_container_->AddChildView(
      std::make_unique<MediaStringView>(this));
  media_string_view_->SetVisible(false);
}

MediaStringView::Settings AmbientSlideshowPeripheralUi::GetSettings() {
  return MediaStringView::Settings(
      {/*icon_light_mode_color=*/ambient::util::GetColor(
           GetColorProvider(), kColorAshIconColorPrimary,
           /*dark_mode_enabled=*/false),
       /*icon_dark_mode_color=*/
       ambient::util::GetColor(GetColorProvider(), kColorAshIconColorPrimary,
                               /*dark_mode_enabled=*/true),
       /*text_light_mode_color=*/
       ambient::util::GetColor(GetColorProvider(), kColorAshTextColorPrimary,
                               /*dark_mode_enabled=*/false),
       /*text_dark_mode_color=*/
       ambient::util::GetColor(GetColorProvider(), kColorAshTextColorPrimary,
                               /*dark_mode_enabled=*/true),
       /*text_shadow_elevation=*/
       ambient::util::kDefaultTextShadowElevation});
}

void AmbientSlideshowPeripheralUi::UpdateGlanceableInfoPosition() {
  gfx::Vector2d jitter = jitter_calculator_->Calculate();
  gfx::Transform transform;
  transform.Translate(jitter);

  DVLOG(4) << "Shifting peripheral ui by " << jitter.ToString();

  ambient_info_view_->SetTextTransform(transform);

  if (media_string_view_->GetVisible()) {
    gfx::Transform media_string_transform;
    media_string_transform.Translate(-jitter.x(), -jitter.y());
    media_string_view_->layer()->SetTransform(media_string_transform);
  }
}

void AmbientSlideshowPeripheralUi::UpdateImageDetails(
    const std::u16string& details,
    const std::u16string& related_details) {
  ambient_info_view_->UpdateImageDetails(details, related_details);
}

BEGIN_METADATA(AmbientSlideshowPeripheralUi, views::View)
END_METADATA

}  // namespace ash
