// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_info_handler.h"

#include "base/strings/string_util.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/request_header/offline_page_header.h"
#include "components/sessions/content/content_serialized_navigation_driver.h"
#include "content/public/browser/navigation_entry.h"

namespace offline_pages {

namespace {
const char kOfflinePageInfoKey[] = "offline";
}

// static
void OfflinePageInfoHandler::Register() {
  std::unique_ptr<OfflinePageInfoHandler> instance(new OfflinePageInfoHandler);
  sessions::ContentSerializedNavigationDriver::GetInstance()
      ->RegisterExtendedInfoHandler(kOfflinePageInfoKey, std::move(instance));
}

OfflinePageInfoHandler::OfflinePageInfoHandler() {}

OfflinePageInfoHandler::~OfflinePageInfoHandler() {}

std::string OfflinePageInfoHandler::GetExtendedInfo(
    content::NavigationEntry* entry) const {
  std::string header_value =
      OfflinePageUtils::ExtractOfflineHeaderValueFromNavigationEntry(entry);
  if (header_value.empty())
    return std::string();

  OfflinePageHeader header(header_value);
  if (!header.need_to_persist)
    return std::string();

  return header_value;
}

void OfflinePageInfoHandler::RestoreExtendedInfo(
    const std::string& info,
    content::NavigationEntry* entry) {
  OfflinePageHeader header(info);
  // Some sanity check.
  if (header.reason == OfflinePageHeader::Reason::NONE ||
      !header.need_to_persist)
    return;
  entry->AddExtraHeaders(header.GetCompleteHeaderString());
}

}  // namespace offline_pages
