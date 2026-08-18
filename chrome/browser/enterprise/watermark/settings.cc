// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/settings.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_protection/features.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace {

constexpr char kTimestampTimezoneTypeHistogram[] =
    "Enterprise.Watermark.TimestampTimezoneType";

// Minimum font size as per WatermarkStyle.yaml schema.
constexpr int kMinFontSize = 1;

// Command line switches that allow users to set fill and outline opacity values
// as a percentage between 0 and 100, inclusive.
constexpr char kWatermarkFillOpacityPercentFlag[] = "watermark-fill-opacity";
constexpr char kWatermarkOutlineOpacityPercentFlag[] =
    "watermark-outline-opacity";

// Helper function to get opacity as a Skia alpha value (0-255)
// from a percentage value (0-100)
// Order of precedence:
// 1. The command line flag value takes precedence over any other settings.
// 2. If the user has set a custom value in the PrefService, that value is used.
// 3. Otherwise, the default value stored in the PrefService is returned.
int GetOpacity(const PrefService* prefs,
               const char* pref_name,
               const char* cmd_opacity_percent_flag,
               int default_percent_value) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(cmd_opacity_percent_flag) &&
      chrome::GetChannel() != version_info::Channel::STABLE &&
      chrome::GetChannel() != version_info::Channel::BETA) {
    int percent_from_flag;
    if (base::StringToInt(cmd->GetSwitchValueASCII(cmd_opacity_percent_flag),
                          &percent_from_flag)) {
      return enterprise_watermark::PercentageToSkAlpha(percent_from_flag);
    }
  }

  if (base::FeatureList::IsEnabled(
          enterprise_watermark::kEnableWatermarkCustomization)) {
    return enterprise_watermark::PercentageToSkAlpha(
        prefs->GetInteger(pref_name));
  }

  return enterprise_watermark::PercentageToSkAlpha(default_percent_value);
}

// Helper function that verifies that the timezone string is valid.
// Returns the default value if the string is invalid.
// Validity is defined by the IANA timezone database.
std::string ResolveTimestampTimezone(const std::string& timezone) {
  if (timezone ==
      enterprise_connectors::kWatermarkStyleTimestampTimezoneDefault) {
    base::UmaHistogramEnumeration(
        kTimestampTimezoneTypeHistogram,
        enterprise_watermark::TimestampTimezoneType::kUserDevice);
    return timezone;
  }

  std::unique_ptr<icu::TimeZone> icu_timezone(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(timezone)));

  if (!icu_timezone || *icu_timezone == icu::TimeZone::getUnknown()) {
    base::UmaHistogramEnumeration(
        kTimestampTimezoneTypeHistogram,
        enterprise_watermark::TimestampTimezoneType::kInvalidFallback);
    return enterprise_watermark::GetDefaultTimestampTimezone();
  }

  base::UmaHistogramEnumeration(
      kTimestampTimezoneTypeHistogram,
      enterprise_watermark::TimestampTimezoneType::kValidIANATimeZone);
  return timezone;
}
}  // namespace

namespace enterprise_watermark {

SkAlpha PercentageToSkAlpha(int percent_value) {
  return std::clamp(percent_value, 0, 100) * 255 / 100;
}

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

int GetDefaultFontSize() {
  return enterprise_connectors::kWatermarkStyleFontSizeDefault;
}

std::string GetDefaultTimestampTimezone() {
  return enterprise_connectors::kWatermarkStyleTimestampTimezoneDefault;
}

SkColor GetFillColor(const PrefService* prefs) {
  int alpha =
      GetOpacity(prefs, enterprise_connectors::kWatermarkStyleFillOpacityPref,
                 kWatermarkFillOpacityPercentFlag,
                 enterprise_connectors::kWatermarkStyleFillOpacityDefault);
  return SkColorSetA(kBaseFillRGB, alpha);
}

SkColor GetOutlineColor(const PrefService* prefs) {
  int alpha = GetOpacity(
      prefs, enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
      kWatermarkOutlineOpacityPercentFlag,
      enterprise_connectors::kWatermarkStyleOutlineOpacityDefault);
  return SkColorSetA(kBaseOutlineRGB, alpha);
}

int GetFontSize(const PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(
          enterprise_watermark::kEnableWatermarkCustomization)) {
    return GetDefaultFontSize();
  }
  int font_size_from_pref =
      prefs->GetInteger(enterprise_connectors::kWatermarkStyleFontSizePref);
  return std::max(font_size_from_pref, kMinFontSize);
}

std::string GetTimestampTimezone(const PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(
          enterprise_data_protection::kEnableWatermarkTimestampTimezone)) {
    return GetDefaultTimestampTimezone();
  }

  const PrefService::Preference* pref = prefs->FindPreference(
      enterprise_connectors::kWatermarkStyleTimestampTimezonePref);

  if (!pref || pref->IsDefaultValue()) {
    base::UmaHistogramEnumeration(kTimestampTimezoneTypeHistogram,
                                  TimestampTimezoneType::kDefault);
    return GetDefaultTimestampTimezone();
  }

  return ResolveTimestampTimezone(prefs->GetString(
      enterprise_connectors::kWatermarkStyleTimestampTimezonePref));
}

}  // namespace enterprise_watermark
