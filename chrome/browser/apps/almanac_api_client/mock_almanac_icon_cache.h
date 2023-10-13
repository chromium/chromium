// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_MOCK_ALMANAC_ICON_CACHE_H_
#define CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_MOCK_ALMANAC_ICON_CACHE_H_

#include "base/functional/callback.h"
#include "chrome/browser/apps/almanac_api_client/almanac_icon_cache.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace apps {

// Mock of the Almanac icon cache.
class MockAlmanacIconCache : public AlmanacIconCache {
 public:
  MockAlmanacIconCache();
  ~MockAlmanacIconCache() override;

  MOCK_METHOD2(GetIcon,
               void(const GURL&, base::OnceCallback<void(const gfx::Image&)>));
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_ALMANAC_API_CLIENT_MOCK_ALMANAC_ICON_CACHE_H_
