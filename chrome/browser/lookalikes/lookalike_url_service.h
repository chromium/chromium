// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace base {
class Clock;
}

// Returns eTLD+1 of |hostname|. This excludes private registries, and returns
// "blogspot.com" for "test.blogspot.com" (blogspot.com is listed as a private
// registry). We do this to be consistent with url_formatter's top domain list
// which doesn't have a notion of private registries.
std::string GetETLDPlusOne(const std::string& hostname);

struct DomainInfo {
  // eTLD+1, used for skeleton and edit distance comparison. Must be ASCII.
  // Empty for non-unique domains, localhost or sites whose eTLD+1 is empty.
  const std::string domain_and_registry;
  // eTLD+1 without the registry part, and with a trailing period. For
  // "www.google.com", this will be "google.". Used for edit distance
  // comparisons. Empty for non-unique domains, localhost or sites whose eTLD+1
  // is empty.
  const std::string domain_without_registry;

  // Result of IDN conversion of domain_and_registry field.
  const url_formatter::IDNConversionResult idn_result;
  // Skeletons of domain_and_registry field.
  const url_formatter::Skeletons skeletons;

  DomainInfo(const std::string& arg_domain_and_registry,
             const std::string& arg_domain_without_registry,
             const url_formatter::IDNConversionResult& arg_idn_result,
             const url_formatter::Skeletons& arg_skeletons);
  ~DomainInfo();
  DomainInfo(const DomainInfo& other);
};

// Returns a DomainInfo instance computed from |url|. Will return empty fields
// for non-unique hostnames (e.g. site.test), localhost or sites whose eTLD+1 is
// empty.
DomainInfo GetDomainInfo(const GURL& url);

// A service that handles operations on lookalike URLs. It can fetch the list of
// engaged sites in a background thread and cache the results until the next
// update. This is more efficient than fetching the list on each navigation for
// each tab separately.
class LookalikeUrlService : public KeyedService {
 public:
  explicit LookalikeUrlService(Profile* profile);
  ~LookalikeUrlService() override;

  using EngagedSitesCallback =
      base::OnceCallback<void(const std::vector<DomainInfo>&)>;

  static LookalikeUrlService* Get(Profile* profile);

  // Returns whether the engaged site list is recently updated.
  bool EngagedSitesNeedUpdating();

  // Triggers an update to the engaged sites list and calls |callback| with the
  // new list once available.
  void ForceUpdateEngagedSites(EngagedSitesCallback callback);

  // Returns the _current_ list of engaged sites, without updating them if
  // they're out of date.
  const std::vector<DomainInfo> GetLatestEngagedSites() const;

  void SetClockForTesting(base::Clock* clock);

 private:
  void OnFetchEngagedSites(EngagedSitesCallback callback,
                           std::vector<mojom::SiteEngagementDetails> details);

  Profile* profile_;
  base::Clock* clock_;
  base::Time last_engagement_fetch_time_;
  std::vector<DomainInfo> engaged_sites_;
  base::WeakPtrFactory<LookalikeUrlService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlService);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
