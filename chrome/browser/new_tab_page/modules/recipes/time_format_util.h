// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_TIME_FORMAT_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_TIME_FORMAT_UTIL_H_

#include <string>

#include "base/time/time.h"

// Turns the |viewed_timestamp| Unix Epoch timestamp of a task visit into a
// human-readable, localized string indicating when this task was viewed (e.g.
// "Viewed today", "Viewed in the past week").
std::string GetViewedItemText(base::Time viewed_timestamp);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_TIME_FORMAT_UTIL_H_
