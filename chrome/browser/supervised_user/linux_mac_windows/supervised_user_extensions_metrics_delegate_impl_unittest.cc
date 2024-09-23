// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_extensions_metrics_delegate_impl.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kInstalledExtensionsCountHistogramName[] =
    "FamilyUser.InstalledExtensionsCount2";
constexpr char kEnabledExtensionsCountHistogramName[] =
    "FamilyUser.EnabledExtensionsCount2";
constexpr char kDisabledExtensionsCountHistogramName[] =
    "FamilyUser.DisabledExtensionsCount2";
}  // namespace

// Tests for family user metrics service.
class SupervisedUserExtensionsMetricsDelegateImplTest
    : public extensions::ExtensionServiceTestBase {
 public:
  SupervisedUserExtensionsMetricsDelegateImplTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::MainThreadType::IO,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME)) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enabled_features.push_back(
        supervised_user::
            kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    ExtensionServiceInitParams params;
    params.profile_is_supervised = true;
    InitializeExtensionService(std::move(params));

    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    CHECK(service);
    service->Init();

    supervised_user_metrics_service_ =
        std::make_unique<supervised_user::SupervisedUserMetricsService>(
            profile()->GetPrefs(), GetURLFilter(),
            std::make_unique<SupervisedUserExtensionsMetricsDelegateImpl>(
                extensions::ExtensionRegistry::Get(profile()), profile()));
    CHECK(supervised_user_metrics_service_);
  }

  void TearDown() override {}

 protected:
  scoped_refptr<const extensions::Extension> MakeExtension(std::string name) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name).Build();
    return extension;
  }

  int GetDayIdPref() {
    return profile()->GetPrefs()->GetInteger(
        prefs::kSupervisedUserMetricsDayId);
  }

  supervised_user::SupervisedUserURLFilter* GetURLFilter() {
    supervised_user::SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_.get());
    return service->GetURLFilter();
  }

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<supervised_user::SupervisedUserMetricsService>
      supervised_user_metrics_service_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the extensions histograms are recorded on each day.
TEST_F(SupervisedUserExtensionsMetricsDelegateImplTest,
       DailyRecordedExtensionsCount) {
  // At the creation of metrics service we record 0 enabled and 0 installed
  // extensions.
  int start_day = GetDayIdPref();
  EXPECT_EQ(supervised_user::SupervisedUserMetricsService::GetDayIdForTesting(
                base::Time::Now()),
            start_day);

  histogram_tester_.ExpectBucketCount(kInstalledExtensionsCountHistogramName,
                                      /*sample=*/0,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kEnabledExtensionsCountHistogramName,
                                      /*sample=*/0,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kDisabledExtensionsCountHistogramName,
                                      /*sample=*/0,
                                      /*expected_count=*/1);

  // Install two extensions and disable one of them.
  auto extension1 = MakeExtension("Extension 1");
  auto extension2 = MakeExtension("Extension 2");
  service()->AddExtension(extension1.get());
  service()->AddExtension(extension2.get());
  service()->DisableExtension(
      extension1->id(), extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);

  // Move to the next day and ensure the extension histograms are recorded.
  task_environment()->FastForwardBy(base::Days(1));
  int new_day = GetDayIdPref();
  // Check that the test takes mimics recording metrics for 2 consecutive days
  // (crbug.com/347993521).
  ASSERT_EQ(new_day, start_day + 1);
  
  EXPECT_EQ(supervised_user::SupervisedUserMetricsService::GetDayIdForTesting(
                base::Time::Now()),
            new_day);
  // All histograms should have a total of 2 entries (1 for each day).
  histogram_tester_.ExpectTotalCount(kInstalledExtensionsCountHistogramName,
                                     /*expected_count=*/2);
  histogram_tester_.ExpectTotalCount(kEnabledExtensionsCountHistogramName,
                                     /*expected_count=*/2);
  histogram_tester_.ExpectTotalCount(kDisabledExtensionsCountHistogramName,
                                     /*expected_count=*/2);
  // We now record 1 enabled, 1 disabled and 2 installed extensions.
  histogram_tester_.ExpectBucketCount(kInstalledExtensionsCountHistogramName,
                                      /*sample=*/2,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kEnabledExtensionsCountHistogramName,
                                      /*sample=*/1,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kDisabledExtensionsCountHistogramName,
                                      /*sample=*/1,
                                      /*expected_count=*/1);
}
