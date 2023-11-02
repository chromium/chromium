// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_MRU_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_MRU_FEATURES_H_

#include <stdint.h>

namespace tab_ranker {

// Identifies how recently the tab was used.
struct MRUFeatures {
  // Zero-based, so this indicates how many of the |total| tabs are more
  // recently used than this tab.
  int index = 0;

  // Total number of tabs considered when calculating MRU index, ie number of
  // non-incognito tabs open.
  int total = 0;
};

}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_MRU_FEATURES_H_
