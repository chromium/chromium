// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "content/public/browser/page.h"

namespace enterprise_data_protection {

static base::Time TimestampToTime(safe_browsing::Timestamp timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

bool UrlSettings::operator==(const UrlSettings& other) const {
  return watermark_text == other.watermark_text &&
         allow_screenshots == other.allow_screenshots;
}

// static
const UrlSettings& UrlSettings::None() {
  static base::NoDestructor<UrlSettings> empty;
  return *empty.get();
}

// static
void DataProtectionPageUserData::UpdateRTLookupResponse(
    content::Page& page,
    const std::string& identifier,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  auto* ud = GetForPage(page);
  if (ud) {
    // UpdateWatermarkStringInSettings() should be called after settings
    // the lookup response.
    ud->set_rt_lookup_response(std::move(rt_lookup_response));
    return;
  }
  CreateForPage(page, identifier, UrlSettings(), std::move(rt_lookup_response));
}

// static
void DataProtectionPageUserData::UpdateDataControlsScreenshotState(
    content::Page& page,
    const std::string& identifier,
    bool allow) {
  auto* ud = GetForPage(page);
  if (ud) {
    ud->data_controls_settings_.allow_screenshots = allow;
    return;
  }

  UrlSettings data_controls_settings;
  data_controls_settings.allow_screenshots = allow;
  CreateForPage(page, identifier, data_controls_settings, nullptr);
}

DataProtectionPageUserData::DataProtectionPageUserData(
    content::Page& page,
    const std::string& identifier,
    UrlSettings data_controls_settings,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response)
    : PageUserData(page),
      identifier_(identifier),
      data_controls_settings_(std::move(data_controls_settings)),
      rt_lookup_response_(std::move(rt_lookup_response)) {}

DataProtectionPageUserData::~DataProtectionPageUserData() = default;

UrlSettings DataProtectionPageUserData::settings() const {
  if (!rt_lookup_response_ || rt_lookup_response_->threat_info().empty()) {
    return data_controls_settings_;
  }
  UrlSettings merged_settings;
  const auto& threat_infos = rt_lookup_response_->threat_info();

  merged_settings.allow_screenshots = data_controls_settings_.allow_screenshots;
  for (const auto& threat_info : threat_infos) {
    if (!threat_info.has_matched_url_navigation_rule()) {
      continue;
    }

    const auto& rule = threat_info.matched_url_navigation_rule();
    if (merged_settings.watermark_text.empty()) {
      merged_settings.watermark_text = GetWatermarkString(identifier_, rule);
    }
    if (merged_settings.allow_screenshots) {
      merged_settings.allow_screenshots = !rule.block_screenshot();
    }
  }

  return merged_settings;
}

PAGE_USER_DATA_KEY_IMPL(DataProtectionPageUserData);

std::string GetWatermarkString(
    const std::string& identifier,
    const safe_browsing::MatchedUrlNavigationRule& rule) {
  if (!rule.has_watermark_message()) {
    return std::string();
  }

  const safe_browsing::MatchedUrlNavigationRule::WatermarkMessage& watermark =
      rule.watermark_message();

  std::string watermark_text = base::StrCat(
      {identifier, "\n",
       base::TimeFormatAsIso8601(TimestampToTime(watermark.timestamp()))});

  if (!watermark.watermark_message().empty()) {
    watermark_text =
        base::StrCat({watermark.watermark_message(), "\n", watermark_text});
  }

  return watermark_text;
}

}  // namespace enterprise_data_protection
