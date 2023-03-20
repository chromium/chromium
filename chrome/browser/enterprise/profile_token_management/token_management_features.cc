// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/profile_token_management/token_management_features.h"

#include "base/feature_list.h"

namespace profile_token_management::features {

BASE_FEATURE(kEnableProfileTokenManagement,
             "EnableProfileTokenManagement",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace profile_token_management::features
