// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_INTERSTITIAL_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_INTERSTITIAL_UTIL_H_

#include <string>
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"

// This namespace contains shared functions for enterprise interstitials
// security pages.
namespace enterprise_connectors {

// Returns the custom message specified by admin in RTLookup response.
std::u16string GetUrlFilteringCustomMessage(
    const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
        unsafe_resources_);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_INTERSTITIAL_UTIL_H_
