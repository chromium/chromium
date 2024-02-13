// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"

#include <array>
#include <string>
#include <string_view>

#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
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
constexpr std::string_view kAffiliationID = "some-affiliation-id";
constexpr std::string_view kAnotherAffiliationID = "another-affiliation-id";

constexpr char kAffiliatedUserEmail[] = "affiliateduser@example.com";
constexpr char kAffiliatedUserGaiaId[] = "1029384756";

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
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
      policy_test_helper_, std::array{kAffiliationID}));
  policy_test_helper_->InstallOwnerKey();

  ASSERT_TRUE(user_policy_.get());
  ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
      user_policy_.get(), account_id_,
      std::array{affiliated_ ? kAffiliationID : kAnotherAffiliationID}));
}

AffiliationTestHelper AffiliationMixin::GetAffiliationTestHelper() const {
  auto* session_manager_client = ash::FakeSessionManagerClient::Get();
  CHECK(session_manager_client);
  return AffiliationTestHelper::CreateForCloud(session_manager_client);
}

}  // namespace policy
