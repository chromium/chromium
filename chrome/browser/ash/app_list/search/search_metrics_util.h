// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_UTIL_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"

namespace app_list {

constexpr char kHistogramPrefix[] = "Apps.AppList.Search.";
constexpr char kSessionHistogramPrefix[] = "Apps.AppList.Search.Session2.";

// Represents possible error states of the metrics observer itself. These
// values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class Error {
  kMissingNotifier = 0,
  kResultNotFound = 1,
  kUntrackedLocation = 2,
  kUntypedResult = 3,
  kMaxValue = kUntypedResult
};

void LogError(Error error);
void LogSessionError(Error error);

std::string GetAppListOpenMethod(ash::AppListShowSource source);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_METRICS_UTIL_H_
