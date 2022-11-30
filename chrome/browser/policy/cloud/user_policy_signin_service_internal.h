// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_INTERNAL_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_INTERNAL_H_

namespace policy {
namespace internal {

// Allows tests to force all |UserPolicySigninService| to prohibit signout
// even when the policy manager is not registered.
extern bool g_force_prohibit_signout_for_tests;

}  // namespace internal
}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_POLICY_SIGNIN_SERVICE_INTERNAL_H_
