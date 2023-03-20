// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_PREFETCH_ORIGIN_DECIDER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_PREFETCH_ORIGIN_DECIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

class PrefService;
class PrefRegistrySimple;

// A browser-scoped class that maintains persistent logic for when origins
// should not be prefetched.
class PrefetchOriginDecider {
 public:
  explicit PrefetchOriginDecider(
      PrefService* pref_service,
      base::Clock* clock = base::DefaultClock::GetInstance());
  ~PrefetchOriginDecider();

  // Registers prefs. Should only be used for storing in a device-local registry
  // (non-syncing).
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // This should be called anytime browsing data is cleared by the user so that
  // the persistent data store can be cleared as well.
  void OnBrowsingDataCleared();

  // Returns true if the given |url|'s origin is eligible to be prefetched, with
  // respect to any previous 503 responses with a retry-after header.
  bool IsOriginOutsideRetryAfterWindow(const GURL& url) const;

  // Records that the given |url| got a 503 response with the given
  // |retry_after| value. Note that the passed |retry_after| value is subject to
  // a maximum value provided by experiment params.
  void ReportOriginRetryAfter(const GURL& url, base::TimeDelta retry_after);

  PrefetchOriginDecider(const PrefetchOriginDecider&) = delete;
  PrefetchOriginDecider& operator=(const PrefetchOriginDecider&) = delete;

 private:
  // These methods serialize and deserialize |origin_retry_afters_| to
  // |pref_service_| in a dictionary value.
  void LoadFromPrefs();
  void SaveToPrefs() const;

  // Erases any expired entries in |origin_retry_afters_|, returning true iff
  // any entries were removed.
  bool ClearPastEntries();

  // Not owned.
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;

  raw_ptr<const base::Clock> clock_;

  // Maps origins to their last known retry_after time.
  std::map<url::Origin, base::Time> origin_retry_afters_;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_PREFETCH_ORIGIN_DECIDER_H_
