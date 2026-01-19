// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace enterprise_auth {

BASE_DECLARE_FEATURE(kEnableExtensibleEnterpriseSSO);

#if BUILDFLAG(IS_MAC)
BASE_DECLARE_FEATURE(kOktaSSO);

BASE_DECLARE_FEATURE_PARAM(std::string, kOktaSsoRequestHeadersAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kOktaSsoResponseHeadersAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kOktaSsoFixedRequestHeaders);
#endif

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_PLATFORM_AUTH_FEATURES_H_
