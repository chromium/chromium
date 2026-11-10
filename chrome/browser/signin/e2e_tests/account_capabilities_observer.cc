// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin::test {

using ::testing::IsEmpty;

AccountCapabilitiesObserver::AccountCapabilitiesObserver(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  CHECK(identity_manager_);
  identity_manager_observation_.Observe(identity_manager);
}

AccountCapabilitiesObserver::~AccountCapabilitiesObserver() = default;

void AccountCapabilitiesObserver::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (account_id_.empty() || info.GetAccountId() != account_id_) {
    return;
  }

  if (!info.GetAccountCapabilities().AreAnyCapabilitiesKnown()) {
    return;
  }

  identity_manager_observation_.Reset();
  capabilities_known_future_.SetValue();
}

void AccountCapabilitiesObserver::WaitForCapabilitiesFetchAndExpectAllKnown(
    CoreAccountId account_id) {
  CHECK(!account_id.empty());
  CHECK(
      identity_manager_observation_.IsObservingSource(identity_manager_.get()));
  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (info.GetAccountCapabilities().AreAnyCapabilitiesKnown()) {
    identity_manager_observation_.Reset();
    ExpectAllCapabilitiesKnown(info.GetAccountCapabilities());
    return;
  }

  account_id_ = account_id;
  ASSERT_TRUE(capabilities_known_future_.Wait())
      << "Timed out waiting for account capabilities fetch.";
  AccountInfo updated_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  ExpectAllCapabilitiesKnown(updated_info.GetAccountCapabilities());
}

// static
void AccountCapabilitiesObserver::ExpectAllCapabilitiesKnown(
    const AccountCapabilities& capabilities) {
  std::vector<std::string> unknown_capabilities;
  for (std::string_view capability_name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    if (capabilities.GetCapabilityByName(capability_name) ==
        Tribool::kUnknown) {
      unknown_capabilities.push_back(
          AccountCapabilities::GetCapabilityDisplayName(capability_name));
    }
  }
  EXPECT_THAT(unknown_capabilities, IsEmpty())
      << "Capabilities remained kUnknown after fetch";
}

}  // namespace signin::test
