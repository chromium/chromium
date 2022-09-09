// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_VALUE_PROVIDER_VALUE_PROVIDER_UTIL_H_
#define CHROME_BROWSER_POLICY_VALUE_PROVIDER_VALUE_PROVIDER_UTIL_H_

#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_service.h"

// Returns policy::PolicyService for `profile`.
policy::PolicyService* GetPolicyService(Profile* profile);

#endif  // CHROME_BROWSER_POLICY_VALUE_PROVIDER_VALUE_PROVIDER_UTIL_H_
