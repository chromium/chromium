// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

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

  // Adds a callback to be executed when all ongoing search prewarms have
  // finished. This must only be called when `HasOnGoingSearchPrewarm()` returns
  // true.
  void AddSearchPrewarmFinishedCallback(base::OnceClosure callback);

  // Called when a search prewarm request starts.
  void OnSearchPrewarmStarted();

  // Called when a search prewarm request finishes (i.e. receives its headers,
  // or fails/is cancelled). If there are no more ongoing prewarms, it will run
  // all the registered callbacks.
  void OnSearchPrewarmFinished();

 private:
  int ongoing_prewarms_ = 0;
  std::vector<base::OnceClosure> on_finished_callbacks_;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_SEARCH_PREWARM_PROGRESS_SERVICE_H_
