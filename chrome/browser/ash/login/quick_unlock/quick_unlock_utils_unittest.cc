// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_unlock {

namespace {

const char kFactorsOptionAll[] = "all";
const char kFactorsOptionPin[] = "PIN";
const char kFactorsOptionFingerprint[] = "FINGERPRINT";

}  // namespace

class QuickUnlockUtilsUnitTest : public testing::TestWithParam<bool> {
 public:
  QuickUnlockUtilsUnitTest(const QuickUnlockUtilsUnitTest&) = delete;
  QuickUnlockUtilsUnitTest& operator=(const QuickUnlockUtilsUnitTest&) = delete;

 protected:
  QuickUnlockUtilsUnitTest()
      : profile_pref_registry_(new user_prefs::PrefRegistrySyncable),
        pref_store_(new TestingPrefStore) {}
  ~QuickUnlockUtilsUnitTest() override = default;

  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kManagedLocalPinAndPassword);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kManagedLocalPinAndPassword);
    }
    RegisterProfilePrefs(profile_pref_registry_.get());
    profile_pref_registry_->RegisterListPref(prefs::kAllowedLocalAuthFactors);
    pref_service_factory_.set_user_prefs(pref_store_);
  }

  void SetValues(base::ListValue quick_unlock_modes,
                 base::ListValue webauthn_factors) {
    pref_store_->SetValue(prefs::kQuickUnlockModeAllowlist,
                          base::Value(std::move(quick_unlock_modes)),
                          /*flags=*/0);
    pref_store_->SetValue(prefs::kWebAuthnFactors,
                          base::Value(std::move(webauthn_factors)),
                          /*flags=*/0);
  }

  void SetAllowedLocalAuthFactors(base::ListValue factors) {
    pref_store_->SetValue(prefs::kAllowedLocalAuthFactors,
                          base::Value(std::move(factors)), /*flags=*/0);
  }

  std::unique_ptr<PrefService> GetPrefService() {
    return pref_service_factory_.Create(profile_pref_registry_.get());
  }

  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> profile_pref_registry_;
  scoped_refptr<TestingPrefStore> pref_store_;
  PrefServiceFactory pref_service_factory_;
};

// Verifies that the quick unlock and webauthn prefs are set to a list including
// "all" when registered.
TEST_P(QuickUnlockUtilsUnitTest, DefaultPrefIsEnableAll) {
  auto pref_service = GetPrefService();
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
}

TEST_P(QuickUnlockUtilsUnitTest, DisableAll) {
  base::ListValue quick_unlock_none;
  base::ListValue webauthn_none;
  SetValues(std::move(quick_unlock_none), std::move(webauthn_none));
  auto pref_service = GetPrefService();
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
  EXPECT_TRUE(IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_TRUE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_TRUE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
}

// The following tests check if that the purpose kAny is allowed when at least
// one of the two prefs include the target auth method. And check if the
// purposes kUnlock and kWebAuthn are independently controlled by the two
// prefs.
TEST_P(QuickUnlockUtilsUnitTest, QuickUnlockAllWebAuthnEmpty) {
  base::ListValue quick_unlock_all;
  quick_unlock_all.Append(kFactorsOptionAll);
  base::ListValue webauthn_none;
  SetValues(std::move(quick_unlock_all), std::move(webauthn_none));
  auto pref_service = GetPrefService();
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_TRUE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
}

TEST_P(QuickUnlockUtilsUnitTest, QuickUnlockEmptyWebAuthnAll) {
  base::ListValue quick_unlock_none;
  base::ListValue webauthn_all;
  webauthn_all.Append(kFactorsOptionAll);
  SetValues(std::move(quick_unlock_none), std::move(webauthn_all));
  auto pref_service = GetPrefService();
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_TRUE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
}

TEST_P(QuickUnlockUtilsUnitTest, QuickUnlockPinWebAuthnFingerprint) {
  base::ListValue quick_unlock_pin;
  quick_unlock_pin.Append(kFactorsOptionPin);
  base::ListValue webauthn_fingerprint;
  webauthn_fingerprint.Append(kFactorsOptionFingerprint);
  SetValues(std::move(quick_unlock_pin), std::move(webauthn_fingerprint));
  auto pref_service = GetPrefService();
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_TRUE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kAny));
  EXPECT_TRUE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
  EXPECT_FALSE(
      IsFingerprintDisabledByPolicy(pref_service.get(), Purpose::kWebAuthn));
}

TEST_P(QuickUnlockUtilsUnitTest, PinEnabledByAuthFactorsPolicy) {
  if (!GetParam()) {
    GTEST_SKIP() << "Test only relevant with flag enabled.";
  }

  base::ListValue quick_unlock_none;
  base::ListValue webauthn_none;
  SetValues(std::move(quick_unlock_none), std::move(webauthn_none));

  base::ListValue allowed_factors;
  allowed_factors.Append("PIN");
  SetAllowedLocalAuthFactors(std::move(allowed_factors));

  auto pref_service = GetPrefService();
  EXPECT_FALSE(IsPinDisabledByPolicy(pref_service.get(), Purpose::kUnlock));
}

INSTANTIATE_TEST_SUITE_P(All, QuickUnlockUtilsUnitTest, testing::Bool());

}  // namespace quick_unlock
}  // namespace ash
