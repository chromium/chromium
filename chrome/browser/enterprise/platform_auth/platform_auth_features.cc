// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace enterprise_auth {

BASE_FEATURE(kExtensibleEnterpriseSSOAsyncCapabilityCheck,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout,
                   &kExtensibleEnterpriseSSOAsyncCapabilityCheck,
                   base::Seconds(10));

BASE_FEATURE(kPingSso, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensibleEnterpriseSsoOriginsFiltering,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise_auth
