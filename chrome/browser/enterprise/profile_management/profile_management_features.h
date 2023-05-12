// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace profile_management {

// Controls whether third-party profile management is enabled.
BASE_DECLARE_FEATURE(kThirdPartyProfileManagement);

}  // namespace profile_management

#endif  // CHROME_BROWSER_ENTERPRISE_PROFILE_MANAGEMENT_PROFILE_MANAGEMENT_FEATURES_H_
