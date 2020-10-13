// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FLASH_LSO_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FLASH_LSO_HELPER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace content {
class BrowserContext;
}

// This class asynchronously fetches information about Flash LSOs and can delete
// them.
class BrowsingDataFlashLSOHelper
    : public base::RefCounted<BrowsingDataFlashLSOHelper> {
 public:
  typedef base::OnceCallback<void(const std::vector<std::string>&)>
      GetSitesWithFlashDataCallback;

  static BrowsingDataFlashLSOHelper* Create(
      content::BrowserContext* browser_context);

  virtual void StartFetching(GetSitesWithFlashDataCallback callback) = 0;
  virtual void DeleteFlashLSOsForSite(const std::string& site,
                                      base::OnceClosure callback) = 0;

 protected:
  friend class base::RefCounted<BrowsingDataFlashLSOHelper>;
  virtual ~BrowsingDataFlashLSOHelper() {}
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FLASH_LSO_HELPER_H_
