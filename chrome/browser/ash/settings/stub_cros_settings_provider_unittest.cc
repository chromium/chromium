// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class StubCrosSettingsProviderTest : public testing::Test {
 protected:
  StubCrosSettingsProviderTest()
      : provider_(new StubCrosSettingsProvider(
            base::BindRepeating(&StubCrosSettingsProviderTest::FireObservers,
                                base::Unretained(this)))) {}

  ~StubCrosSettingsProviderTest() override {}

  void SetUp() override {
    // Reset the observer notification count.
    observer_count_.clear();
  }

  void AssertPref(const std::string& prefName, const base::Value* value) {
    const base::Value* pref = provider_->Get(prefName);
    ASSERT_TRUE(pref);
    ASSERT_EQ(*pref, *value);
  }

  void ExpectObservers(const std::string& prefName, int count) {
    EXPECT_EQ(observer_count_[prefName], count);
  }

  void FireObservers(const std::string& path) {
    observer_count_[path]++;
  }

  std::unique_ptr<StubCrosSettingsProvider> provider_;
  std::map<std::string, int> observer_count_;
};

TEST_F(StubCrosSettingsProviderTest, HandlesSettings) {
  // HandlesSettings should return false for unknown settings.
  ASSERT_TRUE(provider_->HandlesSetting(kDeviceOwner));
  ASSERT_FALSE(provider_->HandlesSetting("no.such.setting"));
}

TEST_F(StubCrosSettingsProviderTest, Defaults) {
  // Verify default values.
  const base::Value kTrueValue(true);
  AssertPref(kAccountsPrefAllowGuest, &kTrueValue);
  AssertPref(kAccountsPrefAllowNewUser, &kTrueValue);
  AssertPref(kAccountsPrefShowUserNamesOnSignIn, &kTrueValue);

  const base::Value kFalseValue(false);
  AssertPref(kAccountsPrefFamilyLinkAccountsAllowed, &kFalseValue);
}

TEST_F(StubCrosSettingsProviderTest, Set) {
  // Setting value and reading it afterwards returns the same value.
  base::Value owner_value("me@owner");
  provider_->Set(kDeviceOwner, owner_value);
  AssertPref(kDeviceOwner, &owner_value);
  ExpectObservers(kDeviceOwner, 1);
}

TEST_F(StubCrosSettingsProviderTest, SetMissing) {
  // Setting is missing initially but is added by |Set|.
  base::Value pref_value("testing");
  ASSERT_FALSE(provider_->Get(kReleaseChannel));
  provider_->Set(kReleaseChannel, pref_value);
  AssertPref(kReleaseChannel, &pref_value);
  ExpectObservers(kReleaseChannel, 1);
}

TEST_F(StubCrosSettingsProviderTest, PrepareTrustedValues) {
  // Should return immediately without invoking the callback.
  base::OnceClosure closure = base::BindOnce([]() { FAIL(); });
  CrosSettingsProvider::TrustedStatus trusted =
      provider_->PrepareTrustedValues(&closure);
  EXPECT_EQ(CrosSettingsProvider::TRUSTED, trusted);
  EXPECT_TRUE(closure);  // The |closure| was not taken or run.
}

}  // namespace ash
