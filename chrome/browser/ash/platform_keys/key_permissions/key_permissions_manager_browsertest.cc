// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/components/kcer/key_permissions.pb.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_util.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::platform_keys {

namespace {

using ::chromeos::platform_keys::KeyAttributeType;
using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;

constexpr char kTestUserEmail[] = "test@example.com";

// Supports waiting for the result of KeyPermissionsService::AllowKeyForUsage.
using AllowKeyForUsageExecutionWaiter = test_util::StatusWaiter;

// Supports waiting for the result of KeyPermissionsService::AllowKeyForUsage.
class IsKeyAllowedForUsageExecutionWaiter
    : public base::test::TestFuture<std::optional<bool>, Status> {
 public:
  std::optional<bool> allowed() { return Get<0>(); }
  Status status() { return Get<1>(); }
};

}  // namespace

class KeyPermissionsManagerBrowserTestBase
    : public MixinBasedInProcessBrowserTest {
 public:
  KeyPermissionsManagerBrowserTestBase() = default;
  KeyPermissionsManagerBrowserTestBase(
      const KeyPermissionsManagerBrowserTestBase& other) = delete;
  KeyPermissionsManagerBrowserTestBase& operator=(
      const KeyPermissionsManagerBrowserTestBase& other) = delete;
  ~KeyPermissionsManagerBrowserTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Call |RequestDevicePolicyUpdate| so policy blobs are prepared in
    // FakeSessionManagerClient.
    auto device_policy_update = device_state_mixin_.RequestDevicePolicyUpdate();

    // Softoken NSS PKCS11 module (used for testing) allows only predefined key
    // attributes to be set and retrieved. Chaps supports setting and retrieving
    // custom attributes.
    PlatformKeysServiceFactory::GetInstance()->SetTestingMode(true);
  }

  void TearDownInProcessBrowserTestFixture() override {
    PlatformKeysServiceFactory::GetInstance()->SetTestingMode(false);

    MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

 protected:
  virtual void WaitForOneTimeMigrationToFinish() = 0;

  virtual TokenId GetToken() const = 0;

  virtual PlatformKeysService* GetPlatformKeysService() = 0;

  virtual KeyPermissionsManager* GetKeyPermissionsManager() = 0;

  std::vector<uint8_t> GenerateKey() {
    base::test::TestFuture<std::vector<uint8_t>,
                           chromeos::platform_keys::Status>
        generate_key_waiter;
    GetPlatformKeysService()->GenerateRSAKey(GetToken(),
                                             /*modulus_length_bits=*/2048,
                                             /*sw_backed=*/false,
                                             generate_key_waiter.GetCallback());
    EXPECT_TRUE(generate_key_waiter.Wait());
    return std::get<std::vector<uint8_t>>(generate_key_waiter.Take());
  }

  // Returns all keys on the token.
  std::vector<std::vector<uint8_t>> GetAllKeys() {
    base::test::TestFuture<std::vector<std::vector<uint8_t>>, Status>
        get_all_keys_waiter;
    GetPlatformKeysService()->GetAllKeys(GetToken(),
                                         get_all_keys_waiter.GetCallback());
    EXPECT_TRUE(get_all_keys_waiter.Wait());
    return std::get<0>(get_all_keys_waiter.Take());
  }

  // Sets |usage| of |public_key| to |allowed| by altering kKeyPermissions key
  // attribute of |public_key|. Note: Since this is a browsertest, Softoken NSS
  // PKCS11 module is used to fake chaps.
  void SetKeyUsageAllowanceInChaps(KeyUsage usage,
                                   bool allowed,
                                   const std::vector<uint8_t>& public_key) {
    chaps::KeyPermissions key_permissions;
    key_permissions.mutable_key_usages()->set_arc(false);
    key_permissions.mutable_key_usages()->set_corporate(false);

    switch (usage) {
      case KeyUsage::kArc:
        key_permissions.mutable_key_usages()->set_arc(allowed);
        break;
      case KeyUsage::kCorporate:
        key_permissions.mutable_key_usages()->set_corporate(allowed);
        break;
    }

    base::test::TestFuture<Status> set_attr_waiter;
    GetPlatformKeysService()->SetAttributeForKey(
        GetToken(), public_key, KeyAttributeType::kKeyPermissions,
        internal::KeyPermissionsProtoToBytes(key_permissions),
        set_attr_waiter.GetCallback());
    ASSERT_TRUE(set_attr_waiter.Wait());

    ASSERT_EQ(set_attr_waiter.Get<Status>(), Status::kSuccess);
  }

  // Checks if |public_key| is allowed for |usage| by checking kKeyPermissions
  // key attribute of |public_key|.
  // Note: Since this is a browsertest, Softoken NSS PKCS11 module is used to
  // fake chaps.
  bool IsKeyAllowedForUsageInChaps(KeyUsage usage,
                                   const std::vector<uint8_t>& public_key) {
    base::test::TestFuture<std::optional<std::vector<uint8_t>>, Status>
        get_attr_waiter;
    GetPlatformKeysService()->GetAttributeForKey(
        GetToken(), public_key, KeyAttributeType::kKeyPermissions,
        get_attr_waiter.GetCallback());
    EXPECT_TRUE(get_attr_waiter.Wait());

    std::optional<std::vector<uint8_t>> attr = get_attr_waiter.Get<0>();
    if (!attr.has_value()) {
      return false;
    }

    chaps::KeyPermissions key_permissions;
    EXPECT_TRUE(
        internal::KeyPermissionsProtoFromBytes(attr.value(), key_permissions));

    switch (usage) {
      case KeyUsage::kArc:
        return key_permissions.key_usages().arc();
      case KeyUsage::kCorporate:
        return key_permissions.key_usages().corporate();
    }
  }

 private:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

class SystemTokenKeyPermissionsManagerBrowserTest
    : public KeyPermissionsManagerBrowserTestBase {
 public:
  SystemTokenKeyPermissionsManagerBrowserTest() = default;
  SystemTokenKeyPermissionsManagerBrowserTest(
      const SystemTokenKeyPermissionsManagerBrowserTest& other) = delete;
  SystemTokenKeyPermissionsManagerBrowserTest& operator=(
      const SystemTokenKeyPermissionsManagerBrowserTest& other) = delete;
  ~SystemTokenKeyPermissionsManagerBrowserTest() override = default;

 protected:
  void WaitForOneTimeMigrationToFinish() override {
    WaitForPrefValue(g_browser_process->local_state(),
                     prefs::kKeyPermissionsOneTimeMigrationDone,
                     base::Value(true));
  }

  KeyPermissionsManager* GetKeyPermissionsManager() override {
    return KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager();
  }

 private:
  TokenId GetToken() const override { return TokenId::kSystem; }

  PlatformKeysService* GetPlatformKeysService() override {
    return PlatformKeysServiceFactory::GetInstance()->GetDeviceWideService();
  }

  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(SystemTokenKeyPermissionsManagerBrowserTest,
                       AllowKeyForUsage_Arc) {
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der = GenerateKey();

  AllowKeyForUsageExecutionWaiter allow_key_for_usage_waiter;
  GetKeyPermissionsManager()->AllowKeyForUsage(
      allow_key_for_usage_waiter.GetCallback(), KeyUsage::kArc,
      public_key_spki_der);
  ASSERT_TRUE(allow_key_for_usage_waiter.Wait());

  // Explicitly modifying arc permission is not allowed so the operation fails.
  EXPECT_EQ(allow_key_for_usage_waiter.status(), Status::kErrorInternal);
}

IN_PROC_BROWSER_TEST_F(SystemTokenKeyPermissionsManagerBrowserTest,
                       AllowKeyForUsage_Corporate) {
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();

  AllowKeyForUsageExecutionWaiter allow_key_for_usage_waiter;
  GetKeyPermissionsManager()->AllowKeyForUsage(
      allow_key_for_usage_waiter.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_1);
  ASSERT_TRUE(allow_key_for_usage_waiter.Wait());

  EXPECT_EQ(allow_key_for_usage_waiter.status(), Status::kSuccess);
  EXPECT_TRUE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, public_key_spki_der_1));
  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, public_key_spki_der_2));

  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kArc, public_key_spki_der_1));
  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kArc, public_key_spki_der_2));
}

IN_PROC_BROWSER_TEST_F(SystemTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_Arc) {
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/true,
                              public_key_spki_der_1);

  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/false,
                              public_key_spki_der_2);

  std::vector<uint8_t> public_key_spki_der_3 = GenerateKey();

  // Check that public_key_spki_der_1 is allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_1);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der_2 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_2);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_2.allowed().value());

  // Check that public_key_spki_der_3 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_3;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_3.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_3);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_3.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_3.allowed().value());
}

// All system token keys are allowed for corporate usage.
IN_PROC_BROWSER_TEST_F(SystemTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_Corporate) {
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();

  // Check that public_key_spki_der_1 is allowed for corporate usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_1);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der_2 is allowed for corporate usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_2);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().value());
}

IN_PROC_BROWSER_TEST_F(SystemTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_ArcAndCorporate) {
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/true,
                              public_key_spki_der);
  SetKeyUsageAllowanceInChaps(KeyUsage::kCorporate, /*allowed=*/true,
                              public_key_spki_der);

  // Check that public_key_spki_der is allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der is allowed for Corporate usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().value());
}

class UserTokenKeyPermissionsManagerBrowserTest
    : public KeyPermissionsManagerBrowserTestBase {
 public:
  UserTokenKeyPermissionsManagerBrowserTest() = default;
  UserTokenKeyPermissionsManagerBrowserTest(
      const UserTokenKeyPermissionsManagerBrowserTest& other) = delete;
  UserTokenKeyPermissionsManagerBrowserTest& operator=(
      const UserTokenKeyPermissionsManagerBrowserTest& other) = delete;
  ~UserTokenKeyPermissionsManagerBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    KeyPermissionsManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Call |RequestPolicyUpdate| even if not setting affiliation IDs so
    // (empty) policy blobs are prepared in FakeSessionManagerClient.
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();
  }

 protected:
  KeyPermissionsManager* GetKeyPermissionsManager() override {
    return KeyPermissionsManagerImpl::GetUserPrivateTokenKeyPermissionsManager(
        ProfileManager::GetActiveUserProfile());
  }

  void WaitForOneTimeMigrationToFinish() override {
    WaitForPrefValue(ProfileManager::GetActiveUserProfile()->GetPrefs(),
                     prefs::kKeyPermissionsOneTimeMigrationDone,
                     base::Value(true));
  }

  void Login() {
    ASSERT_TRUE(login_manager_mixin_.LoginAndWaitForActiveSession(
        LoginManagerMixin::CreateDefaultUserContext(test_user_info_)));
  }

 private:
  TokenId GetToken() const override { return TokenId::kUser; }

  PlatformKeysService* GetPlatformKeysService() override {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    return PlatformKeysServiceFactory::GetInstance()->GetForBrowserContext(
        profile);
  }

  const AccountId test_user_account_id_ = AccountId::FromUserEmailGaiaId(
      kTestUserEmail,
      signin::GetTestGaiaIdForEmail(kTestUserEmail));
  const LoginManagerMixin::TestUserInfo test_user_info_{test_user_account_id_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_info_}};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_account_id_};
};

// Tests that the key permissions manager doesn't allow ARC key usage to be set
// by chrome components. ARC key usage is managed solely by key permissions
// managers.
IN_PROC_BROWSER_TEST_F(UserTokenKeyPermissionsManagerBrowserTest,
                       AllowKeyForUsage_Arc) {
  Login();
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der = GenerateKey();

  AllowKeyForUsageExecutionWaiter allow_key_for_usage_waiter;
  GetKeyPermissionsManager()->AllowKeyForUsage(
      allow_key_for_usage_waiter.GetCallback(), KeyUsage::kArc,
      public_key_spki_der);
  ASSERT_TRUE(allow_key_for_usage_waiter.Wait());

  EXPECT_EQ(allow_key_for_usage_waiter.status(), Status::kErrorInternal);
}

IN_PROC_BROWSER_TEST_F(UserTokenKeyPermissionsManagerBrowserTest,
                       AllowKeyForUsage_Corporate) {
  Login();
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();

  AllowKeyForUsageExecutionWaiter allow_key_for_usage_waiter;
  GetKeyPermissionsManager()->AllowKeyForUsage(
      allow_key_for_usage_waiter.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_1);
  ASSERT_TRUE(allow_key_for_usage_waiter.Wait());

  EXPECT_EQ(allow_key_for_usage_waiter.status(), Status::kSuccess);
  EXPECT_TRUE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, public_key_spki_der_1));
  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, public_key_spki_der_2));

  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kArc, public_key_spki_der_1));
  EXPECT_FALSE(
      IsKeyAllowedForUsageInChaps(KeyUsage::kArc, public_key_spki_der_2));
}

IN_PROC_BROWSER_TEST_F(UserTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_Arc) {
  Login();
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/true,
                              public_key_spki_der_1);

  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/false,
                              public_key_spki_der_2);

  std::vector<uint8_t> public_key_spki_der_3 = GenerateKey();

  // Check that public_key_spki_der_1 is allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_1);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der_2 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_2);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_2.allowed().value());

  // Check that public_key_spki_der_3 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_3;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_3.GetCallback(), KeyUsage::kArc,
      public_key_spki_der_3);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_3.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_3.allowed().value());
}

IN_PROC_BROWSER_TEST_F(UserTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_Corporate) {
  Login();
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der_1 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kCorporate, /*allowed=*/true,
                              public_key_spki_der_1);

  std::vector<uint8_t> public_key_spki_der_2 = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kCorporate, /*allowed=*/false,
                              public_key_spki_der_2);

  std::vector<uint8_t> public_key_spki_der_3 = GenerateKey();

  // Check that public_key_spki_der_1 is allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_1);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der_2 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_2);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_2.allowed().value());

  // Check that public_key_spki_der_3 is not allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_3;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_3.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der_3);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_3.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_3.allowed().has_value());
  EXPECT_FALSE(is_key_allowed_for_usage_waiter_3.allowed().value());
}

IN_PROC_BROWSER_TEST_F(UserTokenKeyPermissionsManagerBrowserTest,
                       IsKeyAllowedForUsage_ArcAndCorporate) {
  Login();
  // Wait for one-time migration to finish so as to make sure that the keys
  // generated in this test are not synchronized into chaps as part of the
  // migration.
  WaitForOneTimeMigrationToFinish();

  std::vector<uint8_t> public_key_spki_der = GenerateKey();
  SetKeyUsageAllowanceInChaps(KeyUsage::kArc, /*allowed=*/true,
                              public_key_spki_der);
  SetKeyUsageAllowanceInChaps(KeyUsage::kCorporate, /*allowed=*/true,
                              public_key_spki_der);

  // Check that public_key_spki_der is allowed for ARC usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_1;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_1.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_1.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_1.allowed().value());

  // Check that public_key_spki_der is allowed for Corporate usage.
  IsKeyAllowedForUsageExecutionWaiter is_key_allowed_for_usage_waiter_2;
  GetKeyPermissionsManager()->IsKeyAllowedForUsage(
      is_key_allowed_for_usage_waiter_2.GetCallback(), KeyUsage::kCorporate,
      public_key_spki_der);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.Wait());
  EXPECT_EQ(is_key_allowed_for_usage_waiter_2.status(), Status::kSuccess);
  ASSERT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().has_value());
  EXPECT_TRUE(is_key_allowed_for_usage_waiter_2.allowed().value());
}

class UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest
    : public UserTokenKeyPermissionsManagerBrowserTest {
 public:
  UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest() = default;
  UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest(
      const UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest& other) =
      delete;
  UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest& operator=(
      const UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest& other) =
      delete;
  ~UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest() override =
      default;

  void SetUpInProcessBrowserTestFixture() override {
    UserTokenKeyPermissionsManagerBrowserTest::
        SetUpInProcessBrowserTestFixture();

    bool one_time_migration_enabled = !content::IsPreTest();
    KeyPermissionsManagerImpl::SetOneTimeMigrationEnabledForTesting(
        one_time_migration_enabled);
  }

 protected:
  // If |get_corporate_keys| is true, returns all corporate keys in
  // |public_key_list| and returns non-corporate keys otherwise.
  // This function uses the profile's pref service to check if a key is
  // corporate.
  std::vector<std::vector<uint8_t>> FilterKeysByCorporateInPref(
      const std::vector<std::vector<uint8_t>>& public_key_list,
      bool get_corporate_keys) {
    std::vector<std::vector<uint8_t>> filtered_list;
    for (auto& public_key : public_key_list) {
      if (internal::IsUserKeyMarkedCorporateInPref(
              public_key, ProfileManager::GetActiveUserProfile()->GetPrefs()) ==
          get_corporate_keys) {
        filtered_list.push_back(public_key);
      }
    }
    return filtered_list;
  }

  void MarkKeyCorporateForActiveUserInPrefs(
      const std::vector<uint8_t>& public_key_spki_der) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    internal::MarkUserKeyCorporateInPref(public_key_spki_der,
                                         profile->GetPrefs());
  }
};

// Generates corporate and non-corporate keys for a user before the one-time
// migration feature is enabled.
IN_PROC_BROWSER_TEST_F(
    UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest,
    PRE_OneTimeMigration) {
  Login();

  // Generate 2 corporate keys.
  for (int i = 0; i < 2; i++) {
    std::vector<uint8_t> public_key_spki_der = GenerateKey();
    MarkKeyCorporateForActiveUserInPrefs(public_key_spki_der);
  }

  // Generate 2 non-corporate keys.
  for (int i = 0; i < 2; i++) {
    std::vector<uint8_t> public_key_spki_der = GenerateKey();
  }
}

// Tests that after enabling the one-time migration feature, all keys that were
// already generated on the user's private token will have their permissions
// migrated to chaps.
IN_PROC_BROWSER_TEST_F(
    UserTokenOneTimeMigrationKeyPermissionsManagerBrowserTest,
    OneTimeMigration) {
  Login();
  WaitForOneTimeMigrationToFinish();

  std::vector<std::vector<uint8_t>> public_key_list = GetAllKeys();
  // The 4 keys generated in PRE_OneTimeMigration should persist even after
  // restarting UI.
  EXPECT_EQ(public_key_list.size(), 4U);

  std::vector<std::vector<uint8_t>> corporate_public_key_list =
      FilterKeysByCorporateInPref(public_key_list, /*get_corporate_keys=*/true);
  EXPECT_EQ(corporate_public_key_list.size(), 2U);

  std::vector<std::vector<uint8_t>> non_corporate_public_key_list =
      FilterKeysByCorporateInPref(public_key_list,
                                  /*get_corporate_keys=*/false);
  EXPECT_EQ(non_corporate_public_key_list.size(), 2U);

  for (const auto& corporate_key : corporate_public_key_list) {
    EXPECT_FALSE(IsKeyAllowedForUsageInChaps(KeyUsage::kArc, corporate_key));
    EXPECT_TRUE(
        IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, corporate_key));
  }

  for (const auto& non_corporate_key : non_corporate_public_key_list) {
    EXPECT_FALSE(
        IsKeyAllowedForUsageInChaps(KeyUsage::kArc, non_corporate_key));
    EXPECT_FALSE(
        IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, non_corporate_key));
  }
}

class SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest
    : public SystemTokenKeyPermissionsManagerBrowserTest {
 public:
  SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest() = default;
  SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest(
      const SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest&
          other) = delete;
  SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest& operator=(
      const SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest&
          other) = delete;
  ~SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest() override =
      default;

  void SetUpInProcessBrowserTestFixture() override {
    SystemTokenKeyPermissionsManagerBrowserTest::
        SetUpInProcessBrowserTestFixture();

    bool one_time_migration_enabled = !content::IsPreTest();
    KeyPermissionsManagerImpl::SetOneTimeMigrationEnabledForTesting(
        one_time_migration_enabled);
  }
};

// Generates corporate keys on the system token the one-time migration feature
// is enabled.
IN_PROC_BROWSER_TEST_F(
    SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest,
    PRE_OneTimeMigration) {
  // Generate 2 corporate keys.
  // Note: before migration, all keys on the system token were implicitly
  // corporate without being marked in prefs.
  for (int i = 0; i < 2; i++) {
    GenerateKey();
  }
}

// Tests that after enabling the one-time migration feature, all keys that were
// already generated on the system token will have their permissions migrated to
// chaps.
IN_PROC_BROWSER_TEST_F(
    SystemTokenOneTimeMigrationKeyPermissionsManagerBrowserTest,
    OneTimeMigration) {
  std::vector<std::vector<uint8_t>> public_key_list = GetAllKeys();
  // The 2 keys generated in PRE_OneTimeMigration should persist even after
  // restarting the UI.
  EXPECT_EQ(public_key_list.size(), 2U);

  WaitForOneTimeMigrationToFinish();

  // All keys on the system slot should have their corporate flag set to true in
  // chaps.
  for (const auto& corporate_key : public_key_list) {
    EXPECT_FALSE(IsKeyAllowedForUsageInChaps(KeyUsage::kArc, corporate_key));
    EXPECT_TRUE(
        IsKeyAllowedForUsageInChaps(KeyUsage::kCorporate, corporate_key));
  }
}

}  // namespace ash::platform_keys
