// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "base/no_destructor.h"
#include "components/enterprise/data_protection/utils.h"
#include "content/public/browser/page.h"

namespace enterprise_data_protection {

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

  UrlSettings settings = GetUrlSettings(identifier_, rt_lookup_response_.get());
  settings.allow_screenshots &= data_controls_settings_.allow_screenshots;

  return settings;
}

PAGE_USER_DATA_KEY_IMPL(DataProtectionPageUserData);

}  // namespace enterprise_data_protection
