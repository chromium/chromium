// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class ClosedTabCache;
class Profile;

class ClosedTabCacheService : public KeyedService {
 public:
  explicit ClosedTabCacheService(Profile* profile);
  ClosedTabCacheService(const ClosedTabCacheService&) = delete;
  ClosedTabCacheService& operator=(const ClosedTabCacheService&) = delete;
  ~ClosedTabCacheService() override;

  ClosedTabCache& closed_tab_cache();

  // KeyedService:
  void Shutdown() override;

 private:
  raw_ptr<Profile> profile_;

  std::unique_ptr<ClosedTabCache> cache_;
};

#endif  // CHROME_BROWSER_SESSIONS_CLOSED_TAB_CACHE_SERVICE_H_
