// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_USER_POLICY_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_USER_POLICY_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/policy_builder.h"

namespace chromeos {

class LocalPolicyTestServerMixin;

// Mixin for setting up user policy for a test user.
// Currently supports setting cached user policy and optionally user policy
// served by local policy test server..
// NOTE: This mixin will set up in-memory FakeSessionManagerClient during setup.
class UserPolicyMixin : public InProcessBrowserTestMixin {
 public:
  UserPolicyMixin(InProcessBrowserTestMixinHost* mixin_host,
                  const AccountId& account_id);
  UserPolicyMixin(InProcessBrowserTestMixinHost* mixin_host,
                  const AccountId& account_id,
                  LocalPolicyTestServerMixin* policy_server);
  ~UserPolicyMixin() override;

  // InProcessBrowserTestMixin:
  void SetUpInProcessBrowserTestFixture() override;

  // Returns a ScopedUserPolicyUpdate object that will update the cached policy
  // values as it goes out of scope. Calling this will ensure that the cached
  // policy blob is set (even if policy remains empty when ScopedPolicyUpdate is
  // done).
  //
  // If called during setup, before steps that initialize session manager,
  // policy change will be deferred until session manager initialization.
  std::unique_ptr<ScopedUserPolicyUpdate> RequestPolicyUpdate();

 private:
  // Creates a file containing public policy signing key that will be used to
  // verify cached user policy. Cached policy will get rejected by chrome if
  // this step is skipped.
  void SetUpUserKeysFile(const std::string& user_key_bits);

  // Sets policy blobs in the fake session manager client.
  void SetUpPolicy();

  // The account ID of the user whose policy is set up by the mixin.
  AccountId account_id_;

  // Whether the mixin should set up policy blobs during setup.
  // Set in RequestCachedPolicyUpdate() is used during test setup (before
  // SetUpInProcessBrowserTestFixture()).
  bool set_policy_in_setup_ = false;

  // Whether the mixin initialized fake session manager client.
  bool session_manager_initialized_ = false;

  // Policy server that can optionally be passed into UserPolicyMixin. If set
  // user policy changes done by RequestPolicyUpdate() will also be forwarded
  // to the policy server.
  LocalPolicyTestServerMixin* policy_server_ = nullptr;

  policy::UserPolicyBuilder user_policy_builder_;

  base::WeakPtrFactory<UserPolicyMixin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserPolicyMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_USER_POLICY_MIXIN_H_
