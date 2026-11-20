// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace enterprise_auth {

// Kill switch. When enabled, the Mac Extensible Enterprise SSO provider checks
// whether the OS can authenticate a URL on a background sequence instead of on
// the UI thread.
BASE_DECLARE_FEATURE(kExtensibleEnterpriseSSOAsyncCapabilityCheck);

// How long a navigation waits for the background capability check before
// continuing without SSO headers.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout);

// Enables extensible enterprise SSO support for the Ping Identity IdP.
BASE_DECLARE_FEATURE(kPingSso);

// Enables origin filtering in the extensible enterprise SSO provider based on
// the `URLs` entry of the SSO extension's MDM payload. Only effective while
// `kPingSso` is enabled; serves as a kill switch to fall back to forwarding
// every request to the SSO extension if filtering breaks an IdP.
BASE_DECLARE_FEATURE(kExtensibleEnterpriseSsoOriginsFiltering);

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
