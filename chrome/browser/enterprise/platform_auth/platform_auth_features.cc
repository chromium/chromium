// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"

#include "build/build_config.h"

namespace enterprise_auth {

BASE_FEATURE(kEnableExtensibleEnterpriseSSO,
             "EnableExtensibleEnterpriseSSO",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace enterprise_auth
