// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/lookalikes/core/lookalike_url_util.h"

class Profile;

namespace base {
class Clock;
}

// A service that handles operations on lookalike URLs. It can fetch the list of
// engaged sites in a background thread and cache the results until the next
// update. This is more efficient than fetching the list on each navigation for
// each tab separately.
class LookalikeUrlService : public KeyedService {
 public:
  explicit LookalikeUrlService(Profile* profile);

  LookalikeUrlService(const LookalikeUrlService&) = delete;
  LookalikeUrlService& operator=(const LookalikeUrlService&) = delete;

  ~LookalikeUrlService() override;

  using EngagedSitesCallback =
      base::OnceCallback<void(const std::vector<DomainInfo>&)>;

  static LookalikeUrlService* Get(Profile* profile);

  // Returns whether the engaged site list is recently updated. Returns true
  // even when an update has already been queued or is in progress.
  bool EngagedSitesNeedUpdating() const;

  // Triggers an update to the engaged site list if one is not already inflight,
  // then schedules |callback| to be called with the new list once available.
  void ForceUpdateEngagedSites(EngagedSitesCallback callback);

  // Returns the _current_ list of engaged sites, without updating them if
  // they're out of date.
  const std::vector<DomainInfo> GetLatestEngagedSites() const;

  void SetClockForTesting(base::Clock* clock);
  base::Clock* clock() const { return clock_; }

 private:
  void OnUpdateEngagedSitesCompleted(std::vector<DomainInfo> new_engaged_sites);

  raw_ptr<Profile> profile_;
  raw_ptr<base::Clock> clock_;
  base::Time last_engagement_fetch_time_;
  std::vector<DomainInfo> engaged_sites_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Indicates that an update to the engaged sites list has been queued. Serves
  // to prevent enqueuing excessive updates.
  bool update_in_progress_ = false;
  std::vector<EngagedSitesCallback> pending_update_complete_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LookalikeUrlService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_H_
