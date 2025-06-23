// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/settings.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace {

// Base RGB colors for the watermark text.
constexpr SkColor kBaseFillRGB = SkColorSetRGB(0x00, 0x00, 0x00);     // Black
constexpr SkColor kBaseOutlineRGB = SkColorSetRGB(0xff, 0xff, 0xff);  // White

// Command line switches that allow users to set fill and outline opacity values
// as a percentage between 0 and 100, inclusive.
constexpr char kWatermarkFillOpacityPercentFlag[] = "watermark-fill-opacity";
constexpr char kWatermarkOutlineOpacityPercentFlag[] =
    "watermark-outline-opacity";

// Helper function to convert a percentage (0-100) to SkAlpha (0-255).
SkAlpha PercentageToSkAlpha(int percent_value) {
  return std::clamp(percent_value, 0, 100) * 255 / 100;
}

// Helper function to get opacity to Skia alpha value (0-255).
// Order of precedence:
// 1. Command-line flag (0-100 percent).
// 2. PrefService preference (0-100 percent).
// 3. Default percentage value (0-100 percent).
int GetOpacity(const PrefService* prefs,
               const char* pref_name,
               const char* cmd_opacity_percent_flag) {
  int percent_value = prefs->GetInteger(pref_name);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(cmd_opacity_percent_flag) &&
      chrome::GetChannel() != version_info::Channel::STABLE &&
      chrome::GetChannel() != version_info::Channel::BETA) {
    int percent_from_flag;
    if (base::StringToInt(cmd->GetSwitchValueASCII(cmd_opacity_percent_flag),
                          &percent_from_flag)) {
      percent_value = percent_from_flag;
    }
  }
  // Clamp the final percentage (0-100) and convert to Skia alpha (0-255).
  return PercentageToSkAlpha(percent_value);
}
}  // namespace

namespace enterprise_watermark {

SkColor GetDefaultFillColor() {
  return SkColorSetA(
      kBaseFillRGB,
      PercentageToSkAlpha(
          enterprise_connectors::kWatermarkStyleFillOpacityDefault));
}

SkColor GetDefaultOutlineColor() {
  return SkColorSetA(
      kBaseOutlineRGB,
      PercentageToSkAlpha(
          enterprise_connectors::kWatermarkStyleOutlineOpacityDefault));
}

SkColor GetFillColor(const PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(
          enterprise_watermark::kEnableWatermarkCustomization)) {
    return GetDefaultFillColor();
  }
  int alpha =
      GetOpacity(prefs, enterprise_connectors::kWatermarkStyleFillOpacityPref,
                 kWatermarkFillOpacityPercentFlag);
  return SkColorSetA(kBaseFillRGB, alpha);
}

SkColor GetOutlineColor(const PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(
          enterprise_watermark::kEnableWatermarkCustomization)) {
    return GetDefaultOutlineColor();
  }
  int alpha = GetOpacity(
      prefs, enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
      kWatermarkOutlineOpacityPercentFlag);
  return SkColorSetA(kBaseOutlineRGB, alpha);
}
}  // namespace enterprise_watermark
