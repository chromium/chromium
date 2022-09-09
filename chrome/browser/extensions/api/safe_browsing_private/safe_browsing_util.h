// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_UTIL_H_

#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"

namespace extensions {

namespace safe_browsing_util {

// Convert referrer to proper format.
api::safe_browsing_private::ReferrerChainEntry ReferrerToReferrerChainEntry(
    const safe_browsing::ReferrerChainEntry& referrer);

}  // namespace safe_browsing_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_UTIL_H_
