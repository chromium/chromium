// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/account_capabilities_observer.h"

#include <string>
#include <string_view>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin::test {
namespace {

using ::testing::Eq;

constexpr char kPrimaryEmail[] = "primary@example.com";
constexpr char kOtherEmail[] = "other@example.com";

void SetAllCapabilities(AccountInfo& account_info, bool value) {
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.SetAllSupportedCapabilities(value);
}

void SetAllCapabilitiesExceptTwo(AccountInfo& account_info) {
  AccountCapabilitiesTestMutator mutator(&account_info);
  for (std::string_view name :
       AccountCapabilitiesTestMutator::GetSupportedAccountCapabilityNames()) {
    AccountCapabilities probe;
    AccountCapabilitiesTestMutator(&probe).SetCapability(std::string(name),
                                                         true);
    if (probe.can_fetch_family_member_info() == Tribool::kUnknown &&
        probe.can_have_email_address_displayed() == Tribool::kUnknown) {
      mutator.SetCapability(std::string(name), true);
    }
  }
}

AccountInfo WithUpdatedFullName(const AccountInfo& account_info,
                                std::string_view full_name) {
  return AccountInfo::Builder(account_info).SetFullName(full_name).Build();
}

// Verifies that `WaitForCapabilitiesFetchAndExpectAllKnown` returns immediately
// without blocking or failing when all supported capabilities are already
// known.
TEST(AccountCapabilitiesObserverTest,
     ReturnsImmediatelyWhenAllCapabilitiesAreAlreadyKnown) {
  base::test::SingleThreadTaskEnvironment task_environment;
  IdentityTestEnvironment identity_test_env;

  AccountInfo account_info =
      identity_test_env.MakeAccountAvailable(kPrimaryEmail);
  SetAllCapabilities(account_info, true);
  identity_test_env.UpdateAccountInfoForAccount(account_info);

  AccountCapabilitiesObserver observer(identity_test_env.identity_manager());
  observer.WaitForCapabilitiesFetchAndExpectAllKnown(
      account_info.GetAccountId());

  AccountInfo updated_info =
      identity_test_env.identity_manager()->FindExtendedAccountInfoByAccountId(
          account_info.GetAccountId());
  EXPECT_THAT(
      updated_info.GetAccountCapabilities()
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Eq(Tribool::kTrue));
}

// Verifies that `WaitForCapabilitiesFetchAndExpectAllKnown` blocks until
// capabilities arrive asynchronously and succeeds when all supported
// capabilities are known.
TEST(AccountCapabilitiesObserverTest,
     WaitsUntilAllCapabilitiesBecomeKnownAsynchronously) {
  base::test::SingleThreadTaskEnvironment task_environment;
  IdentityTestEnvironment identity_test_env;

  AccountInfo account_info =
      identity_test_env.MakeAccountAvailable(kPrimaryEmail);
  AccountCapabilitiesObserver observer(identity_test_env.identity_manager());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        SetAllCapabilities(account_info, true);
        identity_test_env.UpdateAccountInfoForAccount(account_info);
      }));

  observer.WaitForCapabilitiesFetchAndExpectAllKnown(
      account_info.GetAccountId());

  AccountInfo updated_info =
      identity_test_env.identity_manager()->FindExtendedAccountInfoByAccountId(
          account_info.GetAccountId());
  EXPECT_THAT(
      updated_info.GetAccountCapabilities()
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions(),
      Eq(Tribool::kTrue));
}

// Verifies that `WaitForCapabilitiesFetchAndExpectAllKnown` unblocks
// immediately on an asynchronous partial capability fetch instead of hanging,
// and records a failure naming the capabilities that remained
// `Tribool::kUnknown`.
TEST(AccountCapabilitiesObserverTest,
     FailsFastAndLogsUnknownCapabilityNamesOnAsynchronousPartialFetch) {
  base::test::SingleThreadTaskEnvironment task_environment;
  IdentityTestEnvironment identity_test_env;

  AccountInfo account_info =
      identity_test_env.MakeAccountAvailable(kPrimaryEmail);
  AccountCapabilitiesObserver observer(identity_test_env.identity_manager());

  // Populate all capabilities except `kCanFetchFamilyMemberInfoCapabilityName`
  // and `kCanHaveEmailAddressDisplayedCapabilityName` to simulate a partial
  // capability response from the server.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        SetAllCapabilitiesExceptTwo(account_info);
        identity_test_env.UpdateAccountInfoForAccount(account_info);
      }));

  EXPECT_NONFATAL_FAILURE(observer.WaitForCapabilitiesFetchAndExpectAllKnown(
                              account_info.GetAccountId()),
                          "{ \"kCanFetchFamilyMemberInfoCapabilityName\", "
                          "\"kCanHaveEmailAddressDisplayedCapabilityName\" }");
}

// Verifies that `WaitForCapabilitiesFetchAndExpectAllKnown` also fails fast and
// logs the offending capability display names when a partial fetch already
// completed synchronously before `WaitForCapabilitiesFetchAndExpectAllKnown`
// was called.
TEST(AccountCapabilitiesObserverTest,
     FailsFastAndLogsUnknownCapabilityNamesOnSynchronousPartialFetch) {
  base::test::SingleThreadTaskEnvironment task_environment;
  IdentityTestEnvironment identity_test_env;

  AccountInfo account_info =
      identity_test_env.MakeAccountAvailable(kPrimaryEmail);
  SetAllCapabilitiesExceptTwo(account_info);
  identity_test_env.UpdateAccountInfoForAccount(account_info);

  AccountCapabilitiesObserver observer(identity_test_env.identity_manager());

  EXPECT_NONFATAL_FAILURE(observer.WaitForCapabilitiesFetchAndExpectAllKnown(
                              account_info.GetAccountId()),
                          "{ \"kCanFetchFamilyMemberInfoCapabilityName\", "
                          "\"kCanHaveEmailAddressDisplayedCapabilityName\" }");
}

// Verifies that `WaitForCapabilitiesFetchAndExpectAllKnown` ignores capability
// updates for other accounts and non-capability updates for the target account
// while waiting, and safely ignores subsequent updates after unblocking.
TEST(AccountCapabilitiesObserverTest,
     IgnoresOtherAccountsAndEmptyCapabilitiesWhileWaiting) {
  base::test::SingleThreadTaskEnvironment task_environment;
  IdentityTestEnvironment identity_test_env;

  AccountInfo target_account =
      identity_test_env.MakeAccountAvailable(kPrimaryEmail);
  AccountInfo other_account =
      identity_test_env.MakeAccountAvailable(kOtherEmail);
  AccountCapabilitiesObserver observer(identity_test_env.identity_manager());

  bool target_capability_update_sent = false;

  // First task updates a different account with all capabilities known.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        SetAllCapabilities(other_account, true);
        identity_test_env.UpdateAccountInfoForAccount(other_account);
      }));

  // Second task updates the target account's full name while all capabilities
  // for the target account remain `Tribool::kUnknown`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        target_account = WithUpdatedFullName(target_account, "Test User");
        identity_test_env.UpdateAccountInfoForAccount(target_account);
      }));

  // Third task populates all capabilities on the target account, which should
  // unblock `WaitForCapabilitiesFetchAndExpectAllKnown`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        target_capability_update_sent = true;
        SetAllCapabilities(target_account, true);
        identity_test_env.UpdateAccountInfoForAccount(target_account);
      }));

  observer.WaitForCapabilitiesFetchAndExpectAllKnown(
      target_account.GetAccountId());
  EXPECT_TRUE(target_capability_update_sent);

  // Subsequent updates to the target account after the wait completes must be
  // ignored without re-setting the internal future.
  target_account = WithUpdatedFullName(target_account, "Updated Test User");
  identity_test_env.UpdateAccountInfoForAccount(target_account);

  AccountInfo updated_info =
      identity_test_env.identity_manager()->FindExtendedAccountInfoByAccountId(
          target_account.GetAccountId());
  EXPECT_THAT(updated_info.GetFullName(), Eq("Updated Test User"));
}

}  // namespace
}  // namespace signin::test
