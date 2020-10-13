// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"

class MockBrowsingDataFlashLSOHelper : public BrowsingDataFlashLSOHelper {
 public:
  explicit MockBrowsingDataFlashLSOHelper(
      content::BrowserContext* browser_context);

  // BrowsingDataFlashLSOHelper implementation:
  void StartFetching(GetSitesWithFlashDataCallback callback) override;
  void DeleteFlashLSOsForSite(const std::string& site,
                              base::OnceClosure callback) override;

  // Adds a domain sample.
  void AddFlashLSODomain(const std::string& domain);

  // Notifies the callback.
  void Notify();

  // Returns true if the domain list is empty.
  bool AllDeleted();

 protected:
  ~MockBrowsingDataFlashLSOHelper() override;

 private:
  GetSitesWithFlashDataCallback callback_;

  std::vector<std::string> domains_;

  DISALLOW_COPY_AND_ASSIGN(MockBrowsingDataFlashLSOHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_FLASH_LSO_HELPER_H_
