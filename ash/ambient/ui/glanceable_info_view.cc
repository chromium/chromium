// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/glanceable_info_view.h"

#include <memory>
#include <string>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Appearance.
constexpr int kSpacingBetweenTimeAndWeatherDip = 24;
constexpr int kSpacingBetweenWeatherIconAndTempDip = 8;
constexpr int kWeatherIconSizeDip = 32;

// Typography.
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr int kDefaultFontSizeDip = 64;
constexpr int kWeatherTemperatureFontSizeDip = 32;

// Returns the fontlist used for the time text.
const gfx::FontList& GetTimeFontList() {
  return ambient::util::GetDefaultFontlist();
}

// Returns the fontlist used for the temperature text.
gfx::FontList GetWeatherTemperatureFontList() {
  int temperature_font_size_delta =
      kWeatherTemperatureFontSizeDip - kDefaultFontSizeDip;
  return ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
      temperature_font_size_delta);
}

int GetTimeFontDescent() {
  return GetTimeFontList().GetHeight() - GetTimeFontList().GetBaseline();
}

int GetTemperatureFontDescent() {
  return GetWeatherTemperatureFontList().GetHeight() -
         GetWeatherTemperatureFontList().GetBaseline();
}

}  // namespace

GlanceableInfoView::GlanceableInfoView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
  SetID(AssistantViewID::kAmbientGlanceableInfoView);
  auto* backend_model = delegate_->GetAmbientBackendModel();
  backend_model->AddObserver(this);

  InitLayout();

  if (!backend_model->weather_condition_icon().isNull()) {
    // already has weather info, show immediately.
    Show();
  }
}

GlanceableInfoView::~GlanceableInfoView() {
  delegate_->GetAmbientBackendModel()->RemoveObserver(this);
}

const char* GlanceableInfoView::GetClassName() const {
  return "GlanceableInfoView";
}

void GlanceableInfoView::OnWeatherInfoUpdated() {
  Show();
}

void GlanceableInfoView::Show() {
  AmbientBackendModel* ambient_backend_model =
      delegate_->GetAmbientBackendModel();
  weather_condition_icon_->SetImage(
      ambient_backend_model->weather_condition_icon());

  temperature_->SetText(GetTemperatureText());
}

base::string16 GlanceableInfoView::GetTemperatureText() const {
  AmbientBackendModel* ambient_backend_model =
      delegate_->GetAmbientBackendModel();
  if (ambient_backend_model->show_celsius()) {
    return l10n_util::GetStringFUTF16Int(
        IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_CELSIUS,
        static_cast<int>(ambient_backend_model->GetTemperatureInCelsius()));
  }
  return l10n_util::GetStringFUTF16Int(
      IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
      static_cast<int>(ambient_backend_model->temperature_fahrenheit()));
}

void GlanceableInfoView::InitLayout() {
  // The children of |GlanceableInfoView| will be drawn on their own
  // layer instead of the layer of |PhotoView| with a solid black background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);

  gfx::ShadowValues text_shadow_values = ambient::util::GetTextShadowValues();
  gfx::Insets shadow_insets = gfx::ShadowValue::GetMargin(text_shadow_values);

  // Inits the time view.
  time_view_ = AddChildView(std::make_unique<tray::TimeView>(
      ash::tray::TimeView::ClockLayout::HORIZONTAL_CLOCK,
      Shell::Get()->system_tray_model()->clock()));
  time_view_->SetTextFont(GetTimeFontList());
  time_view_->SetTextColor(kTextColor,
                           /*auto_color_readability_enabled=*/false);
  time_view_->SetTextShadowValues(text_shadow_values);
  // Remove the internal spacing in `time_view_` and adjust spacing for shadows.
  time_view_->SetBorder(views::CreateEmptyBorder(
      -kUnifiedTrayTextTopPadding, -kUnifiedTrayTimeLeftPadding, 0,
      kSpacingBetweenTimeAndWeatherDip + shadow_insets.right()));

  // Inits the icon view.
  weather_condition_icon_ = AddChildView(std::make_unique<views::ImageView>());
  const gfx::Size size = gfx::Size(kWeatherIconSizeDip, kWeatherIconSizeDip);
  weather_condition_icon_->SetSize(size);
  weather_condition_icon_->SetImageSize(size);
  constexpr int kIconInternalPaddingDip = 4;
  weather_condition_icon_->SetBorder(views::CreateEmptyBorder(
      0, 0,
      GetTimeFontDescent() - shadow_insets.bottom() - kIconInternalPaddingDip,
      kSpacingBetweenWeatherIconAndTempDip + shadow_insets.left()));

  // Inits the temp view.
  temperature_ = AddChildView(std::make_unique<views::Label>());
  temperature_->SetAutoColorReadabilityEnabled(false);
  temperature_->SetEnabledColor(kTextColor);
  temperature_->SetFontList(GetWeatherTemperatureFontList());
  temperature_->SetShadows(text_shadow_values);
  temperature_->SetBorder(views::CreateEmptyBorder(
      0, 0, GetTimeFontDescent() - GetTemperatureFontDescent(), 0));
}

}  // namespace ash
