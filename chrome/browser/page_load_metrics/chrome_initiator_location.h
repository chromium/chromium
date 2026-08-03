// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_INITIATOR_LOCATION_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_INITIATOR_LOCATION_H_

#include "components/page_load_metrics/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}

// TODO(https://crbug.com/517725655): ChromeInitiatorLocation is currently using
// `int16_t` type as the short term plan, for long term plan resolving the type
// safety, please refer to the document fore more details
// https://docs.google.com/document/d/1d9k-YDEdT35LDVN3BkILyDVqZKlv-b6F0yV_OZRVIQk/edit?resourcekey=0-Jr0Dysk9Cabb0vZlG-ESXg&tab=t.0#heading=h.87ram980i2fc
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ChromeInitiatorLocation)
enum class ChromeInitiatorLocation : page_load_metrics::
    NavigationHandleUserData::InitiatorLocation {
      kOther = 0,
      kBookmarkBar = 1,
      kNewTabPage = 2,
      kOmniboxDirectUrlInput = 3,
      kOmniboxDefaultSearchEngine = 4,
      kLinkClick = 5,
      kMaxValue = kLinkClick
    };
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationInitiatorType)

inline page_load_metrics::NavigationHandleUserData::InitiatorLocation
GetInitiatorLocation(ChromeInitiatorLocation type) {
  return static_cast<
      page_load_metrics::NavigationHandleUserData::InitiatorLocation>(type);
}

inline ChromeInitiatorLocation GetChromeInitiatorLocation(
    page_load_metrics::NavigationHandleUserData::InitiatorLocation type) {
  auto chrome_type = static_cast<ChromeInitiatorLocation>(type);
  CHECK_GE(chrome_type, ChromeInitiatorLocation::kOther);
  CHECK_LE(chrome_type, ChromeInitiatorLocation::kMaxValue);
  return chrome_type;
}

std::string StringifyChromeInitiatorLocation(
    ChromeInitiatorLocation initiator_location);

void AttachNewTabPageNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachOmniboxDirectUrlInputNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachOmniboxDefaultSearchEngineNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

void AttachBookmarkBarNavigationHandleUserData(
    content::NavigationHandle& navigation_handle);

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_CHROME_INITIATOR_LOCATION_H_
