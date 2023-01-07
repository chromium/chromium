// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_INFO_HANDLER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_INFO_HANDLER_H_

#include "components/sessions/content/extended_info_handler.h"

namespace offline_pages {

// Used to parse the extra request header string that defines offline page
// loading behaviors.
//
class OfflinePageInfoHandler : public sessions::ExtendedInfoHandler {
 public:
  // Creates and registers a single instance.
  static void Register();

  OfflinePageInfoHandler();
  ~OfflinePageInfoHandler() override;

  // ExtendedInfoHandler:
  std::string GetExtendedInfo(content::NavigationEntry* entry) const override;
  void RestoreExtendedInfo(const std::string& info,
                           content::NavigationEntry* entry) override;
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_INFO_HANDLER_H_
