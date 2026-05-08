// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_metrics_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicMetricsProviderTest : public testing::Test {
 public:
  void SetUp() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

    profile1_ = profile_manager()->CreateTestingProfile("profile1");
    profile2_ = profile_manager()->CreateTestingProfile("profile2");
  }

  void TearDown() override {
    profile1_ = nullptr;
    profile2_ = nullptr;
    testing_profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

 protected:
  TestingProfileManager* profile_manager() { return testing_profile_manager_; }
  Profile* profile1() { return profile1_; }
  Profile* profile2() { return profile2_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<Profile> profile1_;
  raw_ptr<Profile> profile2_;
};

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData) {
  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  // Should have recorded metrics for both profiles.
  histograms.ExpectTotalCount("Glic.ProfileEnablement.IsEnabled.SteadyState",
                              2);
}

TEST_F(GlicMetricsProviderTest, ProvideCurrentSessionData_ZoomLevel) {
  // Set FRE completion for profile2 only.
  SetFRECompletion(profile2(), prefs::FreStatus::kCompleted);
  // Set zoom level for profile2.
  profile2()->GetPrefs()->SetInteger(prefs::kGlicZoomLevel, 125);

  GlicMetricsProvider provider;
  base::HistogramTester histograms;

  provider.ProvideCurrentSessionData(nullptr);

  // Should have recorded a single sample, as only profile2 completed FRE.
  histograms.ExpectUniqueSample("Glic.ZoomLevel.SteadyState", 125, 1);

  // Set FRE completion for profile1.
  SetFRECompletion(profile1(), prefs::FreStatus::kCompleted);

  provider.ProvideCurrentSessionData(nullptr);
  histograms.ExpectTotalCount("Glic.ZoomLevel.SteadyState", 3);
}

}  // namespace glic
