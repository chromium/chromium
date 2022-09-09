// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_weather_view.h"

#include <memory>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_weather_model.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Size of the weather icon in DIPs.
const int kIconSize = 24;

AmbientWeatherModel* GetWeatherModel() {
  auto* ambient_controller = Shell::Get()->ambient_controller();
  DCHECK(ambient_controller);
  return ambient_controller->GetAmbientWeatherModel();
}

}  // namespace

GlanceablesWeatherView::GlanceablesWeatherView() {
  GetWeatherModel()->AddObserver(this);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  const gfx::Size size(kIconSize, kIconSize);
  icon_->SetSize(size);
  icon_->SetImageSize(size);
  const int kTopPad = 2;
  const int kSpacingBetweenIconAndTemp = 8;
  icon_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kTopPad, 0, 0, kSpacingBetweenIconAndTemp)));

  temperature_ = AddChildView(std::make_unique<views::Label>());
  temperature_->SetAutoColorReadabilityEnabled(false);
  // TODO(crbug.com/1353119): Use color provider and move to OnThemeChanged().
  temperature_->SetEnabledColor(gfx::kGoogleGrey200);
  temperature_->SetFontList(gfx::FontList({"Google Sans"},
                                          gfx::Font::FontStyle::NORMAL, 24,
                                          gfx::Font::Weight::NORMAL));
  // Show a hyphen until the real weather data is fetched.
  temperature_->SetText(u"-");
}

GlanceablesWeatherView::~GlanceablesWeatherView() {
  GetWeatherModel()->RemoveObserver(this);
}

void GlanceablesWeatherView::OnWeatherInfoUpdated() {
  AmbientWeatherModel* model = GetWeatherModel();
  DCHECK(model);

  // Resize the image using RESIZE_BEST quality, which creates a better image
  // than letting views::ImageView resize it.
  gfx::ImageSkia icon_resized = gfx::ImageSkiaOperations::CreateResizedImage(
      model->weather_condition_icon(), skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kIconSize, kIconSize));
  icon_->SetImage(icon_resized);

  // TODO(crbug.com/1353119): Fahrenheit versus celsius settings.
  temperature_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
      static_cast<int>(model->temperature_fahrenheit())));
}

}  // namespace ash
