// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"

#include "build/build_config.h"

namespace enterprise_auth {

BASE_FEATURE(kEnableExtensibleEnterpriseSSO, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enables native SSO support with Okta services.
BASE_FEATURE(kOktaSSO, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace enterprise_auth
