// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cpu_performance_metrics_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/cpu_performance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace metrics {

class CpuPerformanceMetricsProviderTest : public testing::Test {
 public:
  CpuPerformanceMetricsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~CpuPerformanceMetricsProviderTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_ = profile_manager_.CreateTestingProfile(kTestUserEmail);
    profiles::SetLastUsedProfile(profile_->GetBaseName());

#if BUILDFLAG(IS_CHROMEOS)
    auto* fake_user_manager = new ash::FakeChromeUserManager();
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(fake_user_manager));
    const AccountId account_id(AccountId::FromUserEmail(kTestUserEmail));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

 protected:
  static constexpr char kTestUserEmail[] = "test@example.com";

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
#endif
};

TEST_F(CpuPerformanceMetricsProviderTest, ProvideCurrentSessionData) {
  base::HistogramTester histogram_tester;
  CpuPerformanceMetricsProvider provider;
  const auto nominal_tier = content::cpu_performance::GetTier();
  const int nominal_val = static_cast<int>(nominal_tier);

  // Default value (-1) should not log any metrics.
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  histogram_tester.ExpectTotalCount(
      "PerformanceControls.CpuPerformanceTier.UserOverride", 0);
  histogram_tester.ExpectTotalCount(
      "PerformanceControls.CpuPerformanceTier.PolicyOverride", 0);

  // User override should log to UserOverride histogram.
  const int user_override_val =
      (nominal_val + 1) %
      (static_cast<int>(content::cpu_performance::Tier::kMaxValue) + 1);
  const auto user_override_tier =
      static_cast<content::cpu_performance::Tier>(user_override_val);
  profile_->GetTestingPrefService()->SetUserPref(
      prefs::kCpuPerformanceTierOverride, base::Value(user_override_val));
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      "PerformanceControls.CpuPerformanceTier.UserOverride",
      CpuPerformanceMetricsProvider::EncodePair(nominal_tier,
                                                user_override_tier),
      1);
  histogram_tester.ExpectTotalCount(
      "PerformanceControls.CpuPerformanceTier.PolicyOverride", 0);

  // Managed (policy) override should log to PolicyOverride histogram.
  const int policy_override_val =
      (user_override_val + 1) %
      (static_cast<int>(content::cpu_performance::Tier::kMaxValue) + 1);
  const auto policy_override_tier =
      static_cast<content::cpu_performance::Tier>(policy_override_val);
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kCpuPerformanceTierOverride, base::Value(policy_override_val));
  provider.ProvideCurrentSessionData(/*uma_proto=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      "PerformanceControls.CpuPerformanceTier.UserOverride",
      CpuPerformanceMetricsProvider::EncodePair(nominal_tier,
                                                user_override_tier),
      1);
  histogram_tester.ExpectUniqueSample(
      "PerformanceControls.CpuPerformanceTier.PolicyOverride",
      CpuPerformanceMetricsProvider::EncodePair(nominal_tier,
                                                policy_override_tier),
      1);
}

}  // namespace metrics
