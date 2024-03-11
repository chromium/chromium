// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_page_user_data.h"

#include "content/public/browser/page.h"

namespace enterprise_data_protection {

// static
void DataProtectionPageUserData::UpdateDataProtectionState(
    content::Page& page,
    const std::string& watermark_text,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response) {
  auto* ud = GetForPage(page);
  if (ud) {
    ud->set_watermark_text(watermark_text);
    ud->set_rt_lookup_response(std::move(rt_lookup_response));
    return;
  }

  CreateForPage(page, watermark_text, std::move(rt_lookup_response));
}

DataProtectionPageUserData::DataProtectionPageUserData(
    content::Page& page,
    const std::string& watermark_text,
    std::unique_ptr<safe_browsing::RTLookupResponse> rt_lookup_response)
    : PageUserData(page),
      watermark_text_(watermark_text),
      rt_lookup_response_(std::move(rt_lookup_response)) {}

DataProtectionPageUserData::~DataProtectionPageUserData() = default;

PAGE_USER_DATA_KEY_IMPL(DataProtectionPageUserData);

}  // namespace enterprise_data_protection
