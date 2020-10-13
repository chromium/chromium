// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_

#include <list>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"

class Profile;
class BrowsingDataFlashLSOHelper;
class HostContentSettingsMap;

namespace content {
struct StorageUsageInfo;
}

namespace url {
class Origin;
}

namespace storage {
class SpecialStoragePolicy;
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
  void GetQuotaOriginsCallback(const std::set<url::Origin>& origin_set,
                               blink::mojom::StorageType type);
  void SitesWithFlashDataCallback(const std::vector<std::string>& sites);
  void SitesWithMediaLicensesCallback(
      const std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>&
          media_license_info_list);

  void Done(const std::vector<GURL>& origins);

  Profile* profile_;
  base::Time begin_;
  base::Time end_;
  base::OnceCallback<void(int)> completion_callback_;
  int tasks_;
  std::set<std::string> unique_hosts_;
  scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper_;
  scoped_refptr<BrowsingDataMediaLicenseHelper> media_license_helper_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_DATA_COUNTING_HELPER_H_
