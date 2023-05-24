// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace app_list {

void LogError(Error error) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Error"}),
                                error);
}

void LogSessionError(Error error) {
  base::UmaHistogramEnumeration(
      base::StrCat({kSessionHistogramPrefix, "Error"}), error);
}

std::string GetAppListOpenMethod(ash::AppListShowSource source) {
  // This switch determines which metric we submit for the Apps.AppListOpenTime
  // metric. Adding a string requires you update the apps histogram.xml as well.
  switch (source) {
    case ash::AppListShowSource::kSearchKey:
    case ash::AppListShowSource::kSearchKeyFullscreen_DEPRECATED:
      return "SearchKey";
    case ash::AppListShowSource::kShelfButton:
    case ash::AppListShowSource::kShelfButtonFullscreen_DEPRECATED:
      return "HomeButton";
    case ash::AppListShowSource::kSwipeFromShelf:
      return "Swipe";
    case ash::AppListShowSource::kScrollFromShelf:
      return "Scroll";
    case ash::AppListShowSource::kTabletMode:
    case ash::AppListShowSource::kAssistantEntryPoint:
    case ash::AppListShowSource::kBrowser:
    case ash::AppListShowSource::kWelcomeTour:
      return "Others";
  }
}

}  // namespace app_list
