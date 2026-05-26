// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_

#include "base/feature_list.h"

namespace infobars {

// Feature flag controlling the centralization of desktop infobars.
// TODO(https://crbug.com/512837934): Remove feature flag once fully launched
// and all feature-specific delegates are migrated.
BASE_DECLARE_FEATURE(kCentralizedInfoBarFramework);

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_FEATURES_H_
