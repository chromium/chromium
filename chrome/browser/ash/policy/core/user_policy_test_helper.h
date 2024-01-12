// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace ash {
class EmbeddedPolicyTestServerMixin;
}  // namespace ash

namespace enterprise_management {
class CloudPolicySettings;
}

namespace policy {

// This class can be used to apply a user policy to the profile in a
// BrowserTest.
class UserPolicyTestHelper {
 public:
  UserPolicyTestHelper(
      const std::string& account_id,
      ash::EmbeddedPolicyTestServerMixin* embedded_policy_server);

  UserPolicyTestHelper(const UserPolicyTestHelper&) = delete;
  UserPolicyTestHelper& operator=(const UserPolicyTestHelper&) = delete;

  virtual ~UserPolicyTestHelper();

  void SetPolicy(const enterprise_management::CloudPolicySettings& policy);

  // Can be optionally used to wait for the initial policy to be applied to the
  // profile. Alternatively, a login can be simulated, which makes it
  // unnecessary to call this function.
  void WaitForInitialPolicy(Profile* profile);

  // Updates the policy test server with the given policy. Then calls
  // RefreshPolicyAndWait().
  void SetPolicyAndWait(
      const enterprise_management::CloudPolicySettings& policy,
      Profile* profile);

  // Refreshes and waits for the new policy being applied to |profile|.
  void RefreshPolicyAndWait(Profile* profile);

 private:
  const std::string account_id_;
  raw_ptr<ash::EmbeddedPolicyTestServerMixin> embedded_policy_server_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_TEST_HELPER_H_
