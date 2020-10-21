// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_PRELOAD_DATA_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_PRELOAD_DATA_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "chrome/browser/permissions/crowd_deny.pb.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
class FilePath;
}

namespace testing {
class ScopedCrowdDenyPreloadDataOverride;
}

// Stores information relevant for making permission decision on popular sites.
//
// The preloaded list contains reputation data for popular sites, and is
// distributed to Chrome clients ahead of time through the component updater.
// The purpose is to reduce the frequency of on-demand pings to Safe Browsing.
class CrowdDenyPreloadData {
 public:
  using SiteReputation = chrome_browser_crowd_deny::SiteReputation;
  using DomainToReputationMap = base::flat_map<std::string, SiteReputation>;
  using PreloadData = chrome_browser_crowd_deny::PreloadData;

  CrowdDenyPreloadData();
  ~CrowdDenyPreloadData();

  static CrowdDenyPreloadData* GetInstance();

  // Returns preloaded site reputation data for |origin| if available, or
  // nullptr otherwise.
  //
  // Because there is no way to establish the identity of insecure origins,
  // reputation data is only ever provided if |origin| has HTTPS scheme. The
  // port of |origin| is ignored.
  const SiteReputation* GetReputationDataForSite(
      const url::Origin& origin) const;

  // Parses a single instance of chrome_browser_crowd_deny::PreloadData message
  // in binary wire format from the file at |preload_data_path|.
  void LoadFromDisk(const base::FilePath& preload_data_path,
                    const base::Version& version);

  inline bool is_loaded_from_disk() { return is_loaded_from_disk_; }
  inline const base::Optional<base::Version>& version_on_disk() {
    return version_on_disk_;
  }

 private:
  friend class testing::ScopedCrowdDenyPreloadDataOverride;

  void set_site_reputations(DomainToReputationMap map) {
    domain_to_reputation_map_ = std::move(map);
    is_loaded_from_disk_ = true;
  }

  DomainToReputationMap TakeSiteReputations();
  bool is_loaded_from_disk_ = false;
  DomainToReputationMap domain_to_reputation_map_;
  scoped_refptr<base::SequencedTaskRunner> loading_task_runner_;
  base::Optional<base::Version> version_on_disk_;

  DISALLOW_COPY_AND_ASSIGN(CrowdDenyPreloadData);
};

namespace testing {

// Overrides the production preload list, while the instance is in scope, with
// a testing list that is initially empty.
class ScopedCrowdDenyPreloadDataOverride {
 public:
  using SiteReputation = CrowdDenyPreloadData::SiteReputation;
  using DomainToReputationMap = CrowdDenyPreloadData::DomainToReputationMap;

  ScopedCrowdDenyPreloadDataOverride();
  ~ScopedCrowdDenyPreloadDataOverride();

  ScopedCrowdDenyPreloadDataOverride(
      const ScopedCrowdDenyPreloadDataOverride&) = delete;
  ScopedCrowdDenyPreloadDataOverride& operator=(
      const ScopedCrowdDenyPreloadDataOverride&) = delete;

  void SetOriginReputation(const url::Origin& origin,
                           SiteReputation site_reputation);
  void ClearAllReputations();

 private:
  DomainToReputationMap old_map_;
};

}  // namespace testing

#endif  // CHROME_BROWSER_PERMISSIONS_CROWD_DENY_PRELOAD_DATA_H_
