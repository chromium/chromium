// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/appcache_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct StorageUsageInfo;
}

// This class fetches appcache information on behalf of a caller
// on the UI thread.
class BrowsingDataAppCacheHelper
    : public base::RefCountedThreadSafe<BrowsingDataAppCacheHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit BrowsingDataAppCacheHelper(
      content::AppCacheService* appcache_service);

  virtual void StartFetching(FetchCallback completion_callback);
  virtual void DeleteAppCaches(const url::Origin& origin_url);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataAppCacheHelper>;
  virtual ~BrowsingDataAppCacheHelper();

 private:
  // Owned by the profile.
  content::AppCacheService* appcache_service_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataAppCacheHelper);
};

// This class is a thin wrapper around BrowsingDataAppCacheHelper that does not
// fetch its information from the appcache service, but gets them passed when
// called on access.
class CannedBrowsingDataAppCacheHelper : public BrowsingDataAppCacheHelper {
 public:
  explicit CannedBrowsingDataAppCacheHelper(
      content::AppCacheService* appcache_service);

  // Add an appcache to the set of canned caches that is returned by this
  // helper.
  void Add(const url::Origin& origin);

  // Clears the list of canned caches.
  void Reset();

  // True if no appcaches are currently stored.
  bool empty() const;

  // Returns the number of app cache resources.
  size_t GetCount() const;

  // Returns the set or origins with app caches.
  const std::set<url::Origin>& GetOrigins() const;

  // BrowsingDataAppCacheHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteAppCaches(const url::Origin& origin) override;

 private:
  ~CannedBrowsingDataAppCacheHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataAppCacheHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_APPCACHE_HELPER_H_
