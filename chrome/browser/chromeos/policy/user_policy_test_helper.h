// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_USER_POLICY_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_USER_POLICY_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"

class Profile;

namespace base {
class Value;
}

namespace chromeos {
class LocalPolicyTestServerMixin;
}

namespace policy {

// This class can be used to apply a user policy to the profile in a
// BrowserTest.
class UserPolicyTestHelper {
 public:
  UserPolicyTestHelper(
      const std::string& account_id,
      chromeos::LocalPolicyTestServerMixin* local_policy_server);
  virtual ~UserPolicyTestHelper();

  void SetPolicy(const base::Value& mandatory, const base::Value& recommended);

  // Can be optionally used to wait for the initial policy to be applied to the
  // profile. Alternatively, a login can be simulated, which makes it
  // unnecessary to call this function.
  void WaitForInitialPolicy(Profile* profile);

  // Updates the policy test server with the given policy. Then calls
  // RefreshPolicyAndWait().
  void SetPolicyAndWait(const base::Value& mandatory_policy,
                        const base::Value& recommended_policy,
                        Profile* profile);

  // Refreshes and waits for the new policy being applied to |profile|.
  void RefreshPolicyAndWait(Profile* profile);

 private:
  const std::string account_id_;
  chromeos::LocalPolicyTestServerMixin* local_policy_server_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyTestHelper);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_USER_POLICY_TEST_HELPER_H_
