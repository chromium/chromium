// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_
#define CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "build/build_config.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace site_engagement {

// Helper methods for important sites.
// All methods should be used on the UI thread.
class ImportantSitesUtil {
 public:
#if BUILDFLAG(IS_ANDROID)
  static const int kMaxImportantSites = 5;
#else
  static const int kMaxImportantSites = 10;
#endif

  struct ImportantDomainInfo {
    ImportantDomainInfo();
    ~ImportantDomainInfo();
    ImportantDomainInfo(ImportantDomainInfo&&);
    ImportantDomainInfo(const ImportantDomainInfo&) = delete;
    ImportantDomainInfo& operator=(ImportantDomainInfo&&);
    ImportantDomainInfo& operator=(const ImportantDomainInfo&) = delete;
    std::string registerable_domain;
    GURL example_origin;
    double engagement_score = 0;
    int32_t reason_bitfield = 0;
    // Only set if the domain belongs to an installed app.
    std::optional<std::string> app_name;
  };

  // Do not change the values here, as they are used for UMA histograms.
  enum ImportantReason {
    ENGAGEMENT = 0,
    DURABLE = 1,
    BOOKMARKS = 2,
    HOME_SCREEN = 3,
    NOTIFICATIONS = 4,
    REASON_BOUNDARY
  };

  ImportantSitesUtil() = delete;
  ImportantSitesUtil(const ImportantSitesUtil&) = delete;
  ImportantSitesUtil& operator=(const ImportantSitesUtil&) = delete;

  static std::string GetRegisterableDomainOrIP(const GURL& url);

  static std::string GetRegisterableDomainOrIPFromHost(std::string_view host);

  static bool IsDialogDisabled(Profile* profile);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // This returns the top |<=max_results| important registrable domains. This
  // uses site engagement and notifications to generate the list. |max_results|
  // is assumed to be small.
  // See net/base/registry_controlled_domains/registry_controlled_domain.h for
  // more details on registrable domains and the current list of effective
  // eTLDs.
  static std::vector<ImportantDomainInfo> GetImportantRegisterableDomains(
      Profile* profile,
      size_t max_results);

  static std::set<std::string> GetInstalledRegisterableDomains(
      Profile* profile);

  // Record the sites that the user explicitly chose to exclude from clearing
  // (in the Clear Browsing Dialog) and the sites they ignored. This records
  // metrics for excluded and ignored sites and suppresses any 'ignored' sites
  // from appearing in our important sites list if they were ignored 3 times in
  // a row.
  static void RecordExcludedAndIgnoredImportantSites(
      Profile* profile,
      const std::vector<std::string>& excluded_sites,
      const std::vector<int32_t>& excluded_sites_reason_bitfield,
      const std::vector<std::string>& ignored_sites,
      const std::vector<int32_t>& ignored_sites_reason_bitfield);

  // This marks the given origin as important for testing. Note: This changes
  // the score requirements for the Site Engagement Service, so ONLY call for
  // testing.
  static void MarkOriginAsImportantForTesting(Profile* profile,
                                              const GURL& origin);
};

}  // namespace site_engagement

#endif  // CHROME_BROWSER_ENGAGEMENT_IMPORTANT_SITES_UTIL_H_
