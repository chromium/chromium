// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_

#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "url/origin.h"

class Profile;
class HostContentSettingsMap;

namespace content {
struct StorageUsageInfo;
}

namespace storage {
class SpecialStoragePolicy;
struct BucketLocator;
}

// Helper class that counts the number of unique origins, that are affected by
// deleting "cookies and site data" in the CBD dialog.
class SiteDataCountingHelper {
 public:
  explicit SiteDataCountingHelper(
      Profile* profile,
      base::Time begin,
      base::Time end,
      base::OnceCallback<void(int)> completion_callback);
  ~SiteDataCountingHelper();

  void CountAndDestroySelfWhenFinished();

 private:
  void GetOriginsFromHostContentSettignsMap(HostContentSettingsMap* hcsm,
                                            ContentSettingsType type);
  void GetCookiesCallback(const net::CookieList& cookies);
  void GetLocalStorageUsageInfoCallback(
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      const std::vector<content::StorageUsageInfo>& infos);
  void GetCdmStorageCallback(
      const std::vector<std::pair<blink::StorageKey, uint64_t>>&
          usage_per_storage_keys);
  void GetQuotaBucketsCallback(const std::set<storage::BucketLocator>& buckets);
  void GetSharedDictionaryOriginsCallback(
      const std::vector<url::Origin>& origins);

  void Done(const std::vector<GURL>& origins);

  raw_ptr<Profile> profile_;
  base::Time begin_;
  base::Time end_;
  base::OnceCallback<void(int)> completion_callback_;
  int tasks_;
  std::set<std::string> unique_hosts_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_
