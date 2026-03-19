// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/prerender_host_id.h"

// SearchPrewarmProgressService is a keyed service that tracks the progress of
// the ongoing search prewarms. It can be used by throttles or other systems to
// pause requests that may interfere with the prewarm page until the prewarm
// headers are received.
class SearchPrewarmProgressService : public KeyedService {
 public:
  SearchPrewarmProgressService();
  ~SearchPrewarmProgressService() override;

  // Returns true if there is any ongoing search prewarm that hasn't received
  // its response headers.
  bool HasOnGoingSearchPrewarm() const;

  // Returns true if the given PrerenderHostId is tracked by this service.
  bool IsOnGoingSearchPrewarm(content::PrerenderHostId host_id) const;

  // Returns true if the search preloads should be throttled by on-going search
  // prewarm.
  bool ShouldThrottleSearchPreloads() const;

  // Registers a callback to be called when all ongoing search prewarms have
  // finished.
  base::CallbackListSubscription RegisterSearchPrewarmFinishedCallback(
      base::RepeatingClosure callback);

  base::WeakPtr<SearchPrewarmProgressService> GetWeakPtr();

  // Called when a search prewarm request starts.
  void OnSearchPrewarmStarted(content::PrerenderHostId host_id);

  // Called when a search prewarm request finishes (i.e. receives its headers,
  // or fails/is cancelled). If there are no more ongoing prewarms, it will
  // notify all observers.
  void OnSearchPrewarmFinished(content::PrerenderHostId host_id);

 private:
  base::flat_set<content::PrerenderHostId> ongoing_prewarms_;
  base::RepeatingClosureList callbacks_;

  base::WeakPtrFactory<SearchPrewarmProgressService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_
