// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/metrics/bundled_settings_metrics_provider.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

namespace safe_browsing {

// Unit tests for BundledSettingsMetricsProvider.
class BundledSettingsMetricsProviderUnitTest : public testing::Test {
 public:
  BundledSettingsMetricsProviderUnitTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~BundledSettingsMetricsProviderUnitTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile("shelly@gmail.com");
    profiles::SetLastUsedProfile(profile_->GetBaseName());

#if BUILDFLAG(IS_CHROMEOS)
    auto* fake_user_manager = new ash::FakeChromeUserManager();
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(fake_user_manager));
    const AccountId account_id(AccountId::FromUserEmail("shelly@gmail.com"));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

    prefs_ = profile_->GetPrefs();

    SetSelectedBundle(SecuritySettingsBundleSetting::STANDARD);
    SetSafeBrowsingState(prefs_.get(), SafeBrowsingState::STANDARD_PROTECTION);
  }

  void SetSelectedBundle(SecuritySettingsBundleSetting bundle_setting) {
    prefs_->SetInteger(prefs::kSecuritySettingsBundle,
                       static_cast<int>(bundle_setting));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<PrefService> prefs_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
#endif
};

TEST_F(BundledSettingsMetricsProviderUnitTest,
       EnhancedBundle_IsEnhancedBundleSelected) {
  SetSelectedBundle(SecuritySettingsBundleSetting::ENHANCED);

  base::HistogramTester histogram_tester;
  BundledSettingsMetricsProvider provider;
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      "Security.EnhancedBundle.IsEnhancedSelected", true, 1);
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
TEST_F(BundledSettingsMetricsProviderUnitTest,
       StandardBundle_SafeBrowsingMetrics) {
  constexpr char kHistogramName[] =
      "Security.StandardBundle.SafeBrowsingSetting.WasModifiedFromDefault";

  SetSelectedBundle(SecuritySettingsBundleSetting::STANDARD);
  SetSafeBrowsingState(prefs_, SafeBrowsingState::ENHANCED_PROTECTION);

  {
    base::HistogramTester histogram_tester;
    BundledSettingsMetricsProvider provider;
    provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
    histogram_tester.ExpectUniqueSample(kHistogramName, true, 1);
  }

  SetSafeBrowsingState(prefs_, SafeBrowsingState::STANDARD_PROTECTION);
  {
    base::HistogramTester histogram_tester;
    BundledSettingsMetricsProvider provider;
    provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }
}

TEST_F(BundledSettingsMetricsProviderUnitTest,
       EnhancedBundle_SafeBrowsingMetrics) {
  constexpr char kHistogramName[] =
      "Security.EnhancedBundle.SafeBrowsingSetting.WasModifiedFromDefault";

  SetSelectedBundle(SecuritySettingsBundleSetting::ENHANCED);
  SetSafeBrowsingState(prefs_, SafeBrowsingState::STANDARD_PROTECTION);

  {
    base::HistogramTester histogram_tester;
    BundledSettingsMetricsProvider provider;
    provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
    histogram_tester.ExpectUniqueSample(kHistogramName, true, 1);
  }

  SetSafeBrowsingState(prefs_, SafeBrowsingState::ENHANCED_PROTECTION);
  {
    base::HistogramTester histogram_tester;
    BundledSettingsMetricsProvider provider;
    provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
    histogram_tester.ExpectUniqueSample(kHistogramName, false, 1);
  }
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

}  // namespace safe_browsing
