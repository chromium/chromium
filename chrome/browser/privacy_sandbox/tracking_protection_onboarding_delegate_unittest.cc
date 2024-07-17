// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_delegate.h"

#include <stddef.h>

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class ProfileAndDelegate {
 public:
  explicit ProfileAndDelegate(std::unique_ptr<TestingProfile> profile) {
    profile_ = std::move(profile);
    delegate_ =
        std::make_unique<TrackingProtectionOnboardingDelegate>(profile_.get());
  }

  TestingProfile* profile() { return profile_.get(); }
  TrackingProtectionOnboardingDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TrackingProtectionOnboardingDelegate> delegate_;
};

class TrackingProtectionOnboardingDelegateTest : public testing::Test {
 public:
  TrackingProtectionOnboardingDelegateTest() {
    // Standard profile and delegate
    std::unique_ptr<TestingProfile> profile =
        IdentityTestEnvironmentProfileAdaptor::
            CreateProfileForIdentityTestEnvironment();
    profile_and_delegate_ =
        std::make_unique<ProfileAndDelegate>(std::move(profile));

    // Build a new profile, for which we override the browser managed policy
    TestingProfile::Builder builder;
    builder.OverridePolicyConnectorIsManagedForTesting(true);
    std::unique_ptr<TestingProfile> managed_profile = builder.Build();
    profile_and_delegate_managed_ =
        std::make_unique<ProfileAndDelegate>(std::move(managed_profile));
  }

  ProfileAndDelegate* profile_and_delegate() {
    return profile_and_delegate_.get();
  }
  ProfileAndDelegate* profile_and_delegate_managed() {
    return profile_and_delegate_managed_.get();
  }

 private:
  // Needed to ensure tests run on the correct thread.
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<ProfileAndDelegate> profile_and_delegate_;
  std::unique_ptr<ProfileAndDelegate> profile_and_delegate_managed_;
};

TEST_F(TrackingProtectionOnboardingDelegateTest, IsEnterpriseManagedDetection) {
  ASSERT_FALSE(chrome::enterprise_util::IsBrowserManaged(
      profile_and_delegate()->profile()));
  EXPECT_FALSE(profile_and_delegate()->delegate()->IsEnterpriseManaged());

  ASSERT_TRUE(chrome::enterprise_util::IsBrowserManaged(
      profile_and_delegate_managed()->profile()));
  EXPECT_TRUE(
      profile_and_delegate_managed()->delegate()->IsEnterpriseManaged());
}

TEST_F(TrackingProtectionOnboardingDelegateTest, NewProfileDetection) {
  profile_and_delegate()->profile()->SetIsNewProfile(true);
  ASSERT_TRUE(profile_and_delegate()->profile()->IsNewProfile());
  EXPECT_TRUE(profile_and_delegate()->delegate()->IsNewProfile());

  profile_and_delegate()->profile()->SetIsNewProfile(false);
  EXPECT_FALSE(profile_and_delegate()->delegate()->IsNewProfile());
}

TEST_F(TrackingProtectionOnboardingDelegateTest, AreThirdPartyCookiesBlocked) {
  profile_and_delegate()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  EXPECT_FALSE(
      profile_and_delegate()->delegate()->AreThirdPartyCookiesBlocked());

  profile_and_delegate()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  EXPECT_TRUE(
      profile_and_delegate()->delegate()->AreThirdPartyCookiesBlocked());
}
}  // namespace
}  // namespace privacy_sandbox
