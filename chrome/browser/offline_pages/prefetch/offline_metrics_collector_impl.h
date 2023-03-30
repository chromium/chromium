// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {

// TODO(crbug.com/1424920): This is in the process of being deleted.
class OfflineMetricsCollectorImpl {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
