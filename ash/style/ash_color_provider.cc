// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <math.h>

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace ash {

namespace {

// Opacity of the light/dark ink ripple.
constexpr float kLightInkRippleOpacity = 0.08f;
constexpr float kDarkInkRippleOpacity = 0.06f;

// The disabled color is always 38% opacity of the enabled color.
constexpr float kDisabledColorOpacity = 0.38f;

// Color of second tone is always 30% opacity of the color of first tone.
constexpr float kSecondToneOpacity = 0.3f;

// Gets the color mode value from feature flag "--ash-color-mode".
AshColorProvider::AshColorMode GetColorModeFromCommandLine() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (!cl->HasSwitch(switches::kAshColorMode))
    return AshColorProvider::AshColorMode::kDefault;

  const std::string switch_value =
      cl->GetSwitchValueASCII(switches::kAshColorMode);
  if (switch_value == switches::kAshColorModeDark)
    return AshColorProvider::AshColorMode::kDark;

  if (switch_value == switches::kAshColorModeLight)
    return AshColorProvider::AshColorMode::kLight;

  return AshColorProvider::AshColorMode::kDefault;
}

}  // namespace

AshColorProvider::AshColorProvider()
    : color_mode_(GetColorModeFromCommandLine()) {}

AshColorProvider::~AshColorProvider() = default;

// static
AshColorProvider* AshColorProvider::Get() {
  return Shell::Get()->ash_color_provider();
}

// static
SkColor AshColorProvider::GetDisabledColor(SkColor enabled_color) {
  return SkColorSetA(enabled_color, std::round(SkColorGetA(enabled_color) *
                                               kDisabledColorOpacity));
}

// static
SkColor AshColorProvider::GetSecondToneColor(SkColor color_of_first_tone) {
  return SkColorSetA(
      color_of_first_tone,
      std::round(SkColorGetA(color_of_first_tone) * kSecondToneOpacity));
}

SkColor AshColorProvider::DeprecatedGetShieldLayerColor(
    ShieldLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetShieldLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetShieldLayerColor(
    ShieldLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetShieldLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetBaseLayerColor(
    BaseLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetBaseLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetBaseLayerColor(
    BaseLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetBaseLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetControlsLayerColor(
    ControlsLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetControlsLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetControlsLayerColor(
    ControlsLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetControlsLayerColorImpl(type, color_mode);
}

SkColor AshColorProvider::DeprecatedGetContentLayerColor(
    ContentLayerType type,
    SkColor default_color) const {
  if (color_mode_ == AshColorMode::kDefault)
    return default_color;

  return GetContentLayerColorImpl(type, color_mode_);
}

SkColor AshColorProvider::GetContentLayerColor(
    ContentLayerType type,
    AshColorMode given_color_mode) const {
  AshColorMode color_mode =
      color_mode_ != AshColorMode::kDefault ? color_mode_ : given_color_mode;
  DCHECK(color_mode != AshColorMode::kDefault);
  return GetContentLayerColorImpl(type, color_mode);
}

AshColorProvider::RippleAttributes AshColorProvider::GetRippleAttributes(
    SkColor bg_color) const {
  const SkColor base_color = color_utils::GetColorWithMaxContrast(bg_color);
  const float opacity = color_utils::IsDark(base_color)
                            ? kDarkInkRippleOpacity
                            : kLightInkRippleOpacity;
  return RippleAttributes(base_color, opacity, opacity);
}

SkColor AshColorProvider::GetShieldLayerColorImpl(
    ShieldLayerType type,
    AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ShieldLayerType::kAlpha20:
      light_color = SkColorSetA(SK_ColorWHITE, 0x33);  // 20%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x33);
      break;
    case ShieldLayerType::kAlpha40:
      light_color = SkColorSetA(SK_ColorWHITE, 0x66);  // 40%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x66);
      break;
    case ShieldLayerType::kAlpha60:
      light_color = SkColorSetA(SK_ColorWHITE, 0x99);  // 60%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0x99);
      break;
  }
  return color_mode == AshColorMode::kLight ? light_color : dark_color;
}

SkColor AshColorProvider::GetBaseLayerColorImpl(BaseLayerType type,
                                                AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case BaseLayerType::kTransparentWithBlur:
      light_color = SkColorSetA(SK_ColorWHITE, 0xBC);  // 74%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0xBC);
      break;
    case BaseLayerType::kTransparentWithoutBlur:
      light_color = SkColorSetA(SK_ColorWHITE, 0xE6);  // 90%
      dark_color = SkColorSetA(gfx::kGoogleGrey900, 0xE6);
      break;
    case BaseLayerType::kOpaque:
      light_color = SK_ColorWHITE;
      dark_color = gfx::kGoogleGrey900;
      break;
    case BaseLayerType::kRed:
      light_color = gfx::kGoogleRed600;
      dark_color = gfx::kGoogleRed300;
      break;
  }
  return color_mode == AshColorMode::kLight ? light_color : dark_color;
}

SkColor AshColorProvider::GetControlsLayerColorImpl(
    ControlsLayerType type,
    AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ControlsLayerType::kHairlineBorder:
      light_color = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x24);
      break;
    case ControlsLayerType::kInactiveControlBackground:
      light_color = SkColorSetA(SK_ColorBLACK, 0x0D);  // 5%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x1A);   // 10%
      break;
    case ControlsLayerType::kActiveControlBackground:
    case ControlsLayerType::kFocusRing:
      light_color = gfx::kGoogleBlue600;
      dark_color = gfx::kGoogleBlue300;
      break;
  }
  return color_mode == AshColorMode::kLight ? light_color : dark_color;
}

SkColor AshColorProvider::GetContentLayerColorImpl(
    ContentLayerType type,
    AshColorMode color_mode) const {
  SkColor light_color, dark_color;
  switch (type) {
    case ContentLayerType::kSeparator:
      light_color = SkColorSetA(SK_ColorBLACK, 0x24);  // 14%
      dark_color = SkColorSetA(SK_ColorWHITE, 0x24);
      break;
    case ContentLayerType::kTextPrimary:
      light_color = gfx::kGoogleGrey900;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kTextSecondary:
      light_color = gfx::kGoogleGrey700;
      dark_color = gfx::kGoogleGrey500;
      break;
    case ContentLayerType::kIconPrimary:
      light_color = gfx::kGoogleGrey700;
      dark_color = gfx::kGoogleGrey200;
      break;
    case ContentLayerType::kIconSecondary:
      light_color = dark_color = gfx::kGoogleGrey500;
      break;
    case ContentLayerType::kIconRed:
      light_color = gfx::kGoogleRed600;
      dark_color = gfx::kGoogleRed300;
      break;
    case ContentLayerType::kProminentIconButton:
      light_color = gfx::kGoogleBlue600;
      dark_color = gfx::kGoogleBlue300;
      break;
  }
  return color_mode == AshColorMode::kLight ? light_color : dark_color;
}

}  // namespace ash
