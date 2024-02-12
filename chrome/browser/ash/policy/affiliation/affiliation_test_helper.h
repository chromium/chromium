// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_AFFILIATION_AFFILIATION_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_AFFILIATION_AFFILIATION_TEST_HELPER_H_

#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"

class AccountId;

namespace ash {
class FakeSessionManagerClient;
}  // namespace ash

namespace base {
class CommandLine;
}  // namespace base

namespace policy {

class DevicePolicyCrosTestHelper;

class AffiliationTestHelper {
 public:
  // Creates an |AffiliationTestHelper| for Cloud management (regular Google
  // accounts). The |fake_session_manager_client| pointer must outlive this
  // object.
  static AffiliationTestHelper CreateForCloud(
      ash::FakeSessionManagerClient* fake_session_manager_client);

  // Allow move construction, so the static constructors can be used despite
  // deleted constructors.
  AffiliationTestHelper(AffiliationTestHelper&& other);

  AffiliationTestHelper(const AffiliationTestHelper&) = delete;
  AffiliationTestHelper& operator=(const AffiliationTestHelper&) = delete;

  // Sets device affiliation IDs to |device_affiliation_ids| in
  // |fake_session_manager_client_| and modifies |test_helper| so that it
  // contains correct values of device affiliation IDs for future use. To add
  // some device policies and have device affiliation ID valid use |test_helper|
  // modified by this function.
  void SetDeviceAffiliationIDs(
      DevicePolicyCrosTestHelper* test_helper,
      const base::span<const std::string_view>& device_affiliation_ids);

  // Sets user affiliation IDs to |user_affiliation_ids| in
  // |fake_session_manager_client| and modifies |user_policy| so that it
  // contains correct values of user affiliation IDs for future use. To add user
  // policies and have user affiliation IDs valid please use |user_policy|
  // modified by this function.
  void SetUserAffiliationIDs(
      UserPolicyBuilder* user_policy,
      const AccountId& user_account_id,
      const base::span<const std::string_view>& user_affiliation_ids);

  // Registers the user with the given |account_id| on the device and marks OOBE
  // as completed. This method should be called in PRE_* test.
  static void PreLoginUser(const AccountId& account_id);

  // Log in user with |account_id|. User should be registered using
  // PreLoginUser().
  static void LoginUser(const AccountId& user_id);

  // Set necessary for login command line switches. Execute it in
  // SetUpCommandLine().
  static void AppendCommandLineSwitchesForLoginManager(
      base::CommandLine* command_line);

  static const char kFakeRefreshToken[];
  static const char kEnterpriseUserEmail[];
  static const char kEnterpriseUserGaiaId[];

 private:
  explicit AffiliationTestHelper(
      ash::FakeSessionManagerClient* fake_session_manager_client);

  // ASSERTs on pointer validity.
  void CheckPreconditions();

  raw_ptr<ash::FakeSessionManagerClient>
      fake_session_manager_client_;  // Not owned.
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_AFFILIATION_AFFILIATION_TEST_HELPER_H_
