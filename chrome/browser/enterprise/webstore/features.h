// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WEBSTORE_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_WEBSTORE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise::webstore {

// Controls whether webstore navigation throttle is enabled.
BASE_DECLARE_FEATURE(kChromeWebStoreNavigationThrottle);

}  // namespace enterprise::webstore

#endif  // CHROME_BROWSER_ENTERPRISE_WEBSTORE_FEATURES_H_
