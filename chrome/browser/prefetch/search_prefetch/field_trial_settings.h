// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
#define CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_

#include "base/feature_list.h"

extern const base::Feature kSearchPrefetchService;
extern const base::Feature kSearchPrefetchServicePrefetching;

bool SearchPrefetchServiceIsEnabled();

bool SearchPrefetchServicePrefetchingIsEnabled();

#endif  // CHROME_BROWSER_PREFETCH_SEARCH_PREFETCH_FIELD_TRIAL_SETTINGS_H_
