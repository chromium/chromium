// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_PRELOAD_DATA_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_PRELOAD_DATA_H_

#include <memory>
#include <optional>
#include <queue>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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

namespace {
struct PendingOrigin {
  PendingOrigin(
      url::Origin origin,
      base::OnceCallback<void(const chrome_browser_crowd_deny::SiteReputation*)>
          callback);
  ~PendingOrigin();

  url::Origin origin;
  base::OnceCallback<void(const chrome_browser_crowd_deny::SiteReputation*)>
      callback;
};
}  // namespace

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

  using SiteReputationCallback =
      base::OnceCallback<void(const SiteReputation*)>;

  CrowdDenyPreloadData();

  CrowdDenyPreloadData(const CrowdDenyPreloadData&) = delete;
  CrowdDenyPreloadData& operator=(const CrowdDenyPreloadData&) = delete;

  ~CrowdDenyPreloadData();

  static CrowdDenyPreloadData* GetInstance();

  // Delivers preloaded site reputation data for |origin| via |callback|.
  //
  // Because there is no way to establish the identity of insecure origins,
  // reputation data is only ever provided if |origin| has HTTPS scheme. The
  // port of |origin| is ignored.
  void GetReputationDataForSiteAsync(const url::Origin& origin,
                                     SiteReputationCallback callback);

  // Parses a single instance of chrome_browser_crowd_deny::PreloadData message
  // in binary wire format from the file at |preload_data_path|.
  void LoadFromDisk(const base::FilePath& preload_data_path,
                    const base::Version& version);

  inline const std::optional<base::Version>& version_on_disk() {
    return version_on_disk_;
  }

  inline void set_is_ready_to_use_for_testing(bool is_ready) {
    is_ready_to_use_ = is_ready;
  }

  inline int get_pending_origins_queue_size_for_testing() {
    return origins_pending_verification_.size();
  }

  inline bool IsReadyToUse() { return is_ready_to_use_; }

 private:
  friend class testing::ScopedCrowdDenyPreloadDataOverride;
  friend class CrowdDenyPreloadDataTest;

  const SiteReputation* GetReputationDataForSite(
      const url::Origin& origin) const;
  void SetSiteReputations(DomainToReputationMap map);
  void CheckOriginsPendingVerification();
  DomainToReputationMap TakeSiteReputations();
  // The only moment when CrowdDenyPreloadData is not ready to use is during
  // loading from disk.
  bool is_ready_to_use_ = true;
  DomainToReputationMap domain_to_reputation_map_;
  scoped_refptr<base::SequencedTaskRunner> loading_task_runner_;
  std::optional<base::Version> version_on_disk_;
  std::queue<PendingOrigin> origins_pending_verification_;

  base::WeakPtrFactory<CrowdDenyPreloadData> weak_factory_{this};
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
