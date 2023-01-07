// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_
#define CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/memory_details.h"

// Handles asynchronous fetching of memory details and logging histograms about
// memory use of various processes.
// Will run the provided callback when finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  explicit MetricsMemoryDetails(base::OnceClosure callback);

  MetricsMemoryDetails(const MetricsMemoryDetails&) = delete;
  MetricsMemoryDetails& operator=(const MetricsMemoryDetails&) = delete;

 protected:
  ~MetricsMemoryDetails() override;

  // MemoryDetails:
  void OnDetailsAvailable() override;

 private:
  // Updates the global histograms for tracking memory usage.
  void UpdateHistograms();

  void UpdateSiteIsolationMetrics(size_t live_process_count);

  base::OnceClosure callback_;
};

#endif  // CHROME_BROWSER_METRICS_METRICS_MEMORY_DETAILS_H_
