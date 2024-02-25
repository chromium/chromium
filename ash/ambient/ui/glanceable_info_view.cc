// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/glanceable_info_view.h"

#include <memory>
#include <string>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/util/ambient_util.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
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
constexpr int kDefaultFontSizeDip = 64;
constexpr int kWeatherTemperatureFontSizeDip = 32;

// Returns the fontlist used for the time text.
gfx::FontList GetTimeFontList(int font_size_dip) {
  int font_size_delta = font_size_dip - kDefaultFontSizeDip;
  return font_size_delta == 0
             ? ambient::util::GetDefaultFontlist()
             : ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
                   font_size_delta);
}

// Returns the fontlist used for the temperature text.
gfx::FontList GetWeatherTemperatureFontList() {
  int temperature_font_size_delta =
      kWeatherTemperatureFontSizeDip - kDefaultFontSizeDip;
  return ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
      temperature_font_size_delta);
}

int GetFontDescent(const gfx::FontList& font_list) {
  return font_list.GetHeight() - font_list.GetBaseline();
}

int GetTemperatureFontDescent() {
  return GetWeatherTemperatureFontList().GetHeight() -
         GetWeatherTemperatureFontList().GetBaseline();
}

}  // namespace

GlanceableInfoView::GlanceableInfoView(
    AmbientViewDelegate* delegate,
    GlanceableInfoView::Delegate* glanceable_info_view_delegate,
    int time_font_size_dip,
    bool add_text_shadow)
    : delegate_(delegate),
      glanceable_info_view_delegate_(glanceable_info_view_delegate),
      time_font_size_dip_(time_font_size_dip),
      add_text_shadow_(add_text_shadow) {
  DCHECK(delegate);
  DCHECK_GT(time_font_size_dip_, 0);
  SetID(AmbientViewID::kAmbientGlanceableInfoView);
  auto* weather_model = delegate_->GetAmbientWeatherModel();
  scoped_weather_model_observer_.Observe(weather_model);

  InitLayout();

  if (!weather_model->weather_condition_icon().isNull()) {
    // already has weather info, show immediately.
    ShowWeather();
  }
}

GlanceableInfoView::~GlanceableInfoView() = default;

void GlanceableInfoView::OnWeatherInfoUpdated() {
  ShowWeather();
}

void GlanceableInfoView::OnThemeChanged() {
  views::View::OnThemeChanged();
  time_view_->SetTextColor(
      glanceable_info_view_delegate_->GetTimeTemperatureFontColor(),
      /*auto_color_readability_enabled=*/false);
  temperature_->SetEnabledColor(
      glanceable_info_view_delegate_->GetTimeTemperatureFontColor());
  if (add_text_shadow_) {
    gfx::ShadowValues text_shadow_values =
        ambient::util::GetTextShadowValues(GetColorProvider());
    time_view_->SetTextShadowValues(text_shadow_values);
    temperature_->SetShadows(text_shadow_values);
  }
}

void GlanceableInfoView::ShowWeather() {
  AmbientWeatherModel* weather_model = delegate_->GetAmbientWeatherModel();

  // Hide the weather info when the model is incomplete.
  if (weather_model->IsIncomplete()) {
    temperature_->SetText(std::u16string());
    weather_condition_icon_->SetImage(gfx::ImageSkia());
    return;
  }

  // When ImageView has an |image_| with different size than the |image_size_|,
  // it will resize and draw the |image_|. The quality is not as good as if we
  // resize the |image_| to be the same as the |image_size_| with |RESIZE_BEST|
  // method.
  gfx::ImageSkia icon = weather_model->weather_condition_icon();
  gfx::ImageSkia icon_resized = gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kWeatherIconSizeDip, kWeatherIconSizeDip));
  weather_condition_icon_->SetImage(icon_resized);

  temperature_->SetText(GetTemperatureText());
}

std::u16string GlanceableInfoView::GetTemperatureText() const {
  AmbientWeatherModel* weather_model = delegate_->GetAmbientWeatherModel();
  if (weather_model->show_celsius()) {
    return l10n_util::GetStringFUTF16Int(
        IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_CELSIUS,
        static_cast<int>(weather_model->GetTemperatureInCelsius()));
  }
  return l10n_util::GetStringFUTF16Int(
      IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
      static_cast<int>(weather_model->temperature_fahrenheit()));
}

bool GlanceableInfoView::IsWeatherConditionIconSetForTesting() const {
  return !weather_condition_icon_->GetImage().isNull();
}
bool GlanceableInfoView::IsTemperatureSetForTesting() const {
  return !temperature_->GetText().empty();
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

  gfx::Insets shadow_insets;
  if (add_text_shadow_) {
    shadow_insets = gfx::ShadowValue::GetMargin(
        ambient::util::GetTextShadowValues(nullptr));
  }

  // Inits the time view.
  time_view_ = AddChildView(
      std::make_unique<TimeView>(TimeView::ClockLayout::HORIZONTAL_CLOCK,
                                 Shell::Get()->system_tray_model()->clock()));
  gfx::FontList time_font_list = GetTimeFontList(time_font_size_dip_);
  time_view_->SetTextFont(time_font_list);
  // Remove the internal spacing in `time_view_` and adjust spacing for shadows.
  time_view_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      -kUnifiedTrayTextTopPadding, -kUnifiedTrayTimeLeftPadding, 0,
      kSpacingBetweenTimeAndWeatherDip + shadow_insets.right())));

  // Inits the icon view.
  weather_condition_icon_ = AddChildView(std::make_unique<views::ImageView>());
  const gfx::Size size = gfx::Size(kWeatherIconSizeDip, kWeatherIconSizeDip);
  weather_condition_icon_->SetSize(size);
  weather_condition_icon_->SetImageSize(size);
  constexpr int kIconInternalPaddingDip = 4;
  weather_condition_icon_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, 0,
      GetFontDescent(time_font_list) - shadow_insets.bottom() -
          kIconInternalPaddingDip,
      kSpacingBetweenWeatherIconAndTempDip + shadow_insets.left())));

  // Inits the temp view.
  temperature_ = AddChildView(std::make_unique<views::Label>());
  temperature_->SetAutoColorReadabilityEnabled(false);
  temperature_->SetFontList(GetWeatherTemperatureFontList());
  temperature_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, 0, GetFontDescent(time_font_list) - GetTemperatureFontDescent(), 0)));
}

int GlanceableInfoView::GetTimeFontDescent() {
  return GetFontDescent(GetTimeFontList(time_font_size_dip_));
}

BEGIN_METADATA(GlanceableInfoView)
END_METADATA

}  // namespace ash
