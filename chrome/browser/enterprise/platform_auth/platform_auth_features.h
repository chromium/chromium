// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_auth {

// Enables extensible enterprise SSO support for the Ping Identity IdP.
BASE_DECLARE_FEATURE(kPingSso);

// Enables origin filtering in the extensible enterprise SSO provider based on
// the `URLs` entry of the SSO extension's MDM payload. Only effective while
// `kPingSso` is enabled; serves as a kill switch to fall back to forwarding
// every request to the SSO extension if filtering breaks an IdP.
BASE_DECLARE_FEATURE(kExtensibleEnterpriseSsoOriginsFiltering);

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
