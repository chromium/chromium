// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_management/profile_management_features.h"

#include "build/build_config.h"

namespace profile_management::features {

BASE_FEATURE(kThirdPartyProfileManagement,
             "ThirdPartyProfileManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableProfileTokenManagement,
             "EnableProfileTokenManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace profile_management::features
