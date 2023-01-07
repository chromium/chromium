// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"

#include <set>
#include <string>

#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chromeos/ash/components/dbus/authpolicy/authpolicy_client.h"
#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"

namespace policy {

namespace {

// If running with `affiliated==true`, the test will use the same
// `kAffiliationID` as user and device affiliation ID, which makes the user
// affiliated (affiliation IDs overlap).
// If running with `affiliated==false`, the test will use `kAffiliationID` as
// device and `kAnotherAffiliationID` as user affiliation ID, which makes the
// user non-affiliated (affiliation IDs don't overlap).
constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kAnotherAffiliationID[] = "another-affiliation-id";

constexpr char kAffiliatedUserEmail[] = "affiliateduser@example.com";
constexpr char kAffiliatedUserGaiaId[] = "1029384756";
constexpr char kAffiliatedUserObjGuid[] =
    "{11111111-1111-1111-1111-111111111111}";

}  // namespace

AffiliationMixin::AffiliationMixin(
    InProcessBrowserTestMixinHost* host,
    DevicePolicyCrosTestHelper* device_policy_cros_test_helper)
    : InProcessBrowserTestMixin(host),
      policy_test_helper_(device_policy_cros_test_helper),
      account_id_(AccountId::FromUserEmailGaiaId(kAffiliatedUserEmail,
                                                 kAffiliatedUserGaiaId)),
      user_policy_(std::make_unique<UserPolicyBuilder>()) {}

AffiliationMixin::~AffiliationMixin() = default;

void AffiliationMixin::SetUpInProcessBrowserTestFixture() {
  AffiliationTestHelper affiliation_helper = GetAffiliationTestHelper();
  std::set<std::string> device_affiliation_ids;
  device_affiliation_ids.insert(kAffiliationID);
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
      policy_test_helper_, device_affiliation_ids));
  policy_test_helper_->InstallOwnerKey();

  std::set<std::string> user_affiliation_ids;
  if (affiliated_) {
    user_affiliation_ids.insert(kAffiliationID);
  } else {
    user_affiliation_ids.insert(kAnotherAffiliationID);
  }
  ASSERT_TRUE(user_policy_.get());
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
      user_policy_.get(), account_id_, user_affiliation_ids));
}

void AffiliationMixin::SetIsForActiveDirectory(bool is_for_active_directory) {
  if (is_for_active_directory == is_for_active_directory_)
    return;

  is_for_active_directory_ = is_for_active_directory;
  if (is_for_active_directory) {
    account_id_ = AccountId::AdFromUserEmailObjGuid(kAffiliatedUserEmail,
                                                    kAffiliatedUserObjGuid);
  } else {
    account_id_ = AccountId::FromUserEmailGaiaId(kAffiliatedUserEmail,
                                                 kAffiliatedUserGaiaId);
  }
}

AffiliationTestHelper AffiliationMixin::GetAffiliationTestHelper() const {
  auto* session_manager_client = ash::FakeSessionManagerClient::Get();
  CHECK(session_manager_client);
  if (is_for_active_directory_) {
    auto* fake_auth_policy_client = ash::FakeAuthPolicyClient::Get();
    CHECK(fake_auth_policy_client);
    return AffiliationTestHelper::CreateForActiveDirectory(
        session_manager_client, fake_auth_policy_client);
  }
  return AffiliationTestHelper::CreateForCloud(session_manager_client);
}

}  // namespace policy
