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
    ud->UpdateWatermarkStringInSettings(identifier);

    // TODO: Check response for screenshot protection and set as needed.
    return;
  }

  // TODO: Check response for screenshot protection and initialize UrlSettings
  // as needed before passing to CreateForPage().
  CreateForPage(page, identifier, UrlSettings(), std::move(rt_lookup_response));
}

// static
void DataProtectionPageUserData::UpdateScreenshotState(
    content::Page& page,
    const std::string& identifier,
    bool allow) {
  auto* ud = GetForPage(page);
  if (ud) {
    ud->settings_.allow_screenshots = allow;
    return;
  }

  UrlSettings settings;
  settings.allow_screenshots = allow;
  CreateForPage(page, identifier, settings, nullptr);
}

DataProtectionPageUserData::DataProtectionPageUserData(
    content::Page& page,
    const std::string& identifier,
    UrlSettings settings,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response)
    : PageUserData(page),
      settings_(std::move(settings)),
      rt_lookup_response_(std::move(rt_lookup_response)) {
  UpdateWatermarkStringInSettings(identifier);
}

DataProtectionPageUserData::~DataProtectionPageUserData() = default;

void DataProtectionPageUserData::UpdateWatermarkStringInSettings(
    const std::string& identifier) {
  settings_.watermark_text =
      (rt_lookup_response_ && rt_lookup_response_->threat_info_size() > 0)
          ? GetWatermarkString(identifier, rt_lookup_response_->threat_info(0))
          : std::string();
}

PAGE_USER_DATA_KEY_IMPL(DataProtectionPageUserData);

std::string GetWatermarkString(
    const std::string& identifier,
    const safe_browsing::RTLookupResponse::ThreatInfo& threat_info) {
  if (!threat_info.has_matched_url_navigation_rule()) {
    return std::string();
  }

  const safe_browsing::MatchedUrlNavigationRule& rule =
      threat_info.matched_url_navigation_rule();
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
