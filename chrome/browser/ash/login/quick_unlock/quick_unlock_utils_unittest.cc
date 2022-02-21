// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
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

class QuickUnlockUtilsUnitTest : public testing::Test {
 public:
  QuickUnlockUtilsUnitTest(const QuickUnlockUtilsUnitTest&) = delete;
  QuickUnlockUtilsUnitTest& operator=(const QuickUnlockUtilsUnitTest&) = delete;

 protected:
  QuickUnlockUtilsUnitTest()
      : profile_pref_registry_(new user_prefs::PrefRegistrySyncable),
        pref_store_(new TestingPrefStore) {}
  ~QuickUnlockUtilsUnitTest() override {}

  void SetUp() override {
    RegisterProfilePrefs(profile_pref_registry_.get());
    pref_service_factory_.set_user_prefs(pref_store_);
  }

  void SetValues(base::Value quick_unlock_modes, base::Value webauthn_factors) {
    pref_store_->SetValue(
        prefs::kQuickUnlockModeAllowlist,
        std::make_unique<base::Value>(std::move(quick_unlock_modes)),
        /*flags=*/0);
    pref_store_->SetValue(
        prefs::kWebAuthnFactors,
        std::make_unique<base::Value>(std::move(webauthn_factors)),
        /*flags=*/0);
  }

  std::unique_ptr<PrefService> GetPrefService() {
    return pref_service_factory_.Create(profile_pref_registry_.get());
  }

  scoped_refptr<user_prefs::PrefRegistrySyncable> profile_pref_registry_;
  scoped_refptr<TestingPrefStore> pref_store_;
  PrefServiceFactory pref_service_factory_;
};

// Verifies that the quick unlock and webauthn prefs are set to a list including
// "all" when registered.
TEST_F(QuickUnlockUtilsUnitTest, DefaultPrefIsEnableAll) {
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

TEST_F(QuickUnlockUtilsUnitTest, DisableAll) {
  base::Value::ListStorage quick_unlock_none;
  base::Value::ListStorage webauthn_none;
  SetValues(base::Value(quick_unlock_none), base::Value(webauthn_none));
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
TEST_F(QuickUnlockUtilsUnitTest, QuickUnlockAllWebAuthnEmpty) {
  base::Value::ListStorage quick_unlock_all;
  quick_unlock_all.emplace_back(kFactorsOptionAll);
  base::Value::ListStorage webauthn_none;
  SetValues(base::Value(quick_unlock_all), base::Value(webauthn_none));
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

TEST_F(QuickUnlockUtilsUnitTest, QuickUnlockEmptyWebAuthnAll) {
  base::Value::ListStorage quick_unlock_none;
  base::Value::ListStorage webauthn_all;
  webauthn_all.emplace_back(kFactorsOptionAll);
  SetValues(base::Value(quick_unlock_none), base::Value(webauthn_all));
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

TEST_F(QuickUnlockUtilsUnitTest, QuickUnlockPinWebAuthnFingerprint) {
  base::Value::ListStorage quick_unlock_pin;
  quick_unlock_pin.emplace_back(kFactorsOptionPin);
  base::Value::ListStorage webauthn_fingerprint;
  webauthn_fingerprint.emplace_back(kFactorsOptionFingerprint);
  SetValues(base::Value(quick_unlock_pin), base::Value(webauthn_fingerprint));
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

}  // namespace quick_unlock
}  // namespace ash
