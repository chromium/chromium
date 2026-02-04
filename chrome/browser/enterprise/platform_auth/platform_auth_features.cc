// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace enterprise_auth {

BASE_FEATURE(kEnableExtensibleEnterpriseSSO, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enables native SSO support with Okta services.
BASE_FEATURE(kOktaSSO, base::FEATURE_DISABLED_BY_DEFAULT);

// Allowlist for request headers on the Okta SSO URL request.
// Header names must be lowercase. The list is comma-separated.
// If this list is empty all request headers will be allowed.
BASE_FEATURE_PARAM(std::string,
                   kOktaSsoRequestHeadersAllowlist,
                   &kOktaSSO,
                   "OktaSsoRequestHeadersAllowlist",
                   "accept,accept-language,content-type,user-agent,x-okta-user-"
                   "agent-extended");

// Allowlist for response headers on the Okta SSO URL request.
// Header names must be lowercase. The list is comma-separated.
// If this list is empty all response headers will be allowed.
BASE_FEATURE_PARAM(
    std::string,
    kOktaSsoResponseHeadersAllowlist,
    &kOktaSSO,
    "OktaSsoResponseHeadersAllowlist",
    "accept-ch,access-control-allow-credentials,access-control-allow-headers,"
    "access-control-allow-origin,cache-control,challengerequest,content-"
    "security-policy,content-security-policy-report-only,content-type,date,"
    "expires,referrer-policy,server,strict-transport-security,vary,www-"
    "authenticate,x-content-type-options,x-okta-request-id,x-rate-limit-limit,"
    "x-rate-limit-remaining,x-robots-tag");

// Fixed response headers appended to the Okta SSO URL request response.
// Format: list of pipe-separated pairs. Values within a pair are
// semicolon-separated.
BASE_FEATURE_PARAM(
    std::string,
    kOktaSsoFixedRequestHeaders,
    &kOktaSSO,
    "OktaSsoFixedResponseHeaders",
    "Cache-Control;no-cache|Pragma;no-cache|Priority;u=1, "
    "i|Sec-Fetch-Dest;empty|Sec-Fetch-Mode;cors|Sec-Fetch-Site;same-origin");

// The pattern for a SSO URL request path specific to the Okta IdP.
// Format: segments separated with |/|. * is a wildcard matching 1 segment.
BASE_FEATURE_PARAM(
    std::string,
    kOktaSsoURLPattern,
    &kOktaSSO,
    "OktaSsoURLPattern",
    "/idp/idx/authenticators/sso_extension/transactions/*/verify");
#endif

}  // namespace enterprise_auth
