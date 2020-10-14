// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_app_metrics.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);
constexpr char kStartTime[] = "1 Jan 2020 21:15";

apps::mojom::AppPtr MakeApp(const char* app_id,
                            const char* name,
                            base::Time last_launch_time,
                            apps::mojom::InstallSource install_source,
                            apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->name = name;
  app->last_launch_time = last_launch_time;
  app->install_source = install_source;
  app->app_type = app_type;
  return app;
}

}  // namespace

// Tests for family user app metrics.
class FamilyUserAppMetricsTest
    : public extensions::ExtensionServiceTestWithInstall,
      public testing::WithParamInterface<bool> {
 public:
  FamilyUserAppMetricsTest()
      : extensions::ExtensionServiceTestWithInstall(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::MainThreadType::IO,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME)) {}

  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment()->AdvanceClock(forward_by);

    bool profile_is_supervised = GetParam();
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.profile_is_supervised = profile_is_supervised;
    InitializeExtensionService(params);

    EXPECT_EQ(profile_is_supervised, profile()->IsChild());

    supervised_user_service()->Init();
    supervised_user_service()
        ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(true);

    PowerManagerClient::InitializeFake();
    ConstructFamilyUserMetricsService();
  }

  void TearDown() override {
    ShutdownFamilyUserMetricsService();
    PowerManagerClient::Shutdown();
  }

  void InstallExtensions() {
    // Install and enable a theme, which doesn't require parent approval.
    base::FilePath path = data_dir().AppendASCII("theme.crx");
    const extensions::Extension* extension1 = InstallCRX(path, INSTALL_NEW);
    ASSERT_TRUE(extension1);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension1->id()));
    EXPECT_FALSE(
        extensions::Manifest::IsComponentLocation(extension1->location()));

    // Install an extension, but keep it disabled pending parent approval if the
    // current user is supervised.
    path = data_dir().AppendASCII("good.crx");
    bool profile_is_supervised = GetParam();
    InstallState expected_state =
        profile_is_supervised ? INSTALL_WITHOUT_LOAD : INSTALL_NEW;
    const extensions::Extension* extension2 = InstallCRX(path, expected_state);
    ASSERT_TRUE(extension2);
    EXPECT_EQ(profile_is_supervised,
              registry()->disabled_extensions().Contains(extension2->id()));
    EXPECT_NE(profile_is_supervised,
              registry()->enabled_extensions().Contains(extension2->id()));
    EXPECT_FALSE(
        extensions::Manifest::IsComponentLocation(extension2->location()));

    // Install an extension, and approve it if the current user is supervised.
    path = data_dir().AppendASCII("good2048.crx");
    const extensions::Extension* extension3 = InstallCRX(path, expected_state);
    ASSERT_TRUE(extension3);
    if (profile_is_supervised) {
      supervised_user_service()->UpdateApprovedExtensionForTesting(
          extension3->id(),
          SupervisedUserService::ApprovedExtensionChange::kAdd);
    }
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension3->id()));
    EXPECT_FALSE(
        extensions::Manifest::IsComponentLocation(extension3->location()));
  }

  void InstallApps() {
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache =
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->AppRegistryCache();
    deltas.push_back(MakeApp(/*app_id=*/"a", /*app_name=*/"apple",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::InstallSource::kUser,
                             apps::mojom::AppType::kArc));
    deltas.push_back(MakeApp(/*app_id=*/"b", /*app_name=*/"banana",
                             /*last_launch_time=*/base::Time::Now() - kOneDay,
                             apps::mojom::InstallSource::kUser,
                             apps::mojom::AppType::kCrostini));
    deltas.push_back(MakeApp(
        /*app_id=*/"c", /*app_name=*/"cherry",
        /*last_launch_time=*/base::Time::Now() - 7 * kOneDay,
        apps::mojom::InstallSource::kUser, apps::mojom::AppType::kExtension));
    deltas.push_back(MakeApp(
        /*app_id=*/"d", /*app_name=*/"dragon",
        /*last_launch_time=*/base::Time::Now() - 14 * kOneDay,
        apps::mojom::InstallSource::kUser, apps::mojom::AppType::kWeb));
    deltas.push_back(MakeApp(
        /*app_id=*/"e", /*app_name=*/"elderberry",
        /*last_launch_time=*/base::Time::Now() - 21 * kOneDay,
        apps::mojom::InstallSource::kUser, apps::mojom::AppType::kBorealis));
    deltas.push_back(MakeApp(
        /*app_id=*/"f", /*app_name=*/"fig",
        /*last_launch_time=*/base::Time::Now() - 27 * kOneDay,
        apps::mojom::InstallSource::kUser, apps::mojom::AppType::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"g", /*app_name=*/"grape",
        /*last_launch_time=*/base::Time::Now(),
        apps::mojom::InstallSource::kSystem, apps::mojom::AppType::kBuiltIn));
    // Not recorded. This app was launched one day too long ago.
    deltas.push_back(MakeApp(
        /*app_id=*/"h", /*app_name=*/"huckleberry",
        /*last_launch_time=*/base::Time::Now() - 28 * kOneDay,
        apps::mojom::InstallSource::kUser, apps::mojom::AppType::kLacros));
    cache.OnApps(std::move(deltas));
  }

  void ConstructFamilyUserMetricsService() {
    family_user_metrics_service_ =
        std::make_unique<FamilyUserMetricsService>(profile());
  }

  void ShutdownFamilyUserMetricsService() {
    family_user_metrics_service_->Shutdown();
  }

  SupervisedUserService* supervised_user_service() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

 private:
  // We need this member variable, even if it's unused, so
  // FamilyUserSessionMetrics doesn't crash.
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FamilyUserMetricsService> family_user_metrics_service_;
};

// Tests the UMA metrics that count the number of installed and enabled
// extensions and themes.
TEST_P(FamilyUserAppMetricsTest, CountInstalledAndEnabledExtensions) {
  base::HistogramTester histogram_tester;

  InstallExtensions();
  task_environment()->FastForwardBy(kOneDay);
  ShutdownFamilyUserMetricsService();

  // There should be 2 installed extensions and one theme.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kInstalledExtensionsCountHistogramName,
      /*sample=*/3, /*expected_count=*/1);

  bool profile_is_supervised = GetParam();
  if (profile_is_supervised) {
    // There should be 1 enabled extension and a theme.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::kEnabledExtensionsCountHistogramName,
        /*sample=*/2, /*expected_count=*/1);
  } else {
    // Regular user case.
    // There should be 2 enabled extensions and a theme.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::kEnabledExtensionsCountHistogramName,
        /*sample=*/3, /*expected_count=*/1);
  }
}

// Tests the UMA metrics that count the number of recently used apps for
// supervised and regular users.
TEST_P(FamilyUserAppMetricsTest, CountRecentlyUsedApps) {
  base::HistogramTester histogram_tester;

  InstallApps();
  task_environment()->FastForwardBy(kOneDay);
  ShutdownFamilyUserMetricsService();

  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kOtherAppsCountHistogramName,
      /*sample=*/2, /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kArcAppsCountHistogramName, /*sample=*/1,
      /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kBorealisAppsCountHistogramName, /*sample=*/1,
      /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kCrostiniAppsCountHistogramName,
      /*sample=*/1, /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kExtensionAppsCountHistogramName,
      /*sample=*/1, /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kWebAppsCountHistogramName, /*sample=*/1,
      /*expected_counter=*/1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kTotalAppsCountHistogramName,
      /*sample=*/7, /*expected_counter=*/1);
}

// Tests that metrics recording only happens on sign out, and not necessarily
// once per day. Tests that metrics recording happens at most once per day.
TEST_P(FamilyUserAppMetricsTest, FastForwardTwoDays) {
  base::HistogramTester histogram_tester;

  InstallExtensions();
  InstallApps();

  // End time is 3 Jan 2020 21:15.
  task_environment()->FastForwardBy(kOneDay * 2);

  // Metrics recorded here.
  ShutdownFamilyUserMetricsService();

  // Only one snapshot was recorded.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kInstalledExtensionsCountHistogramName,
      /*sample=*/3, /*expected_count=*/1);
  // One app has not been used within 28 days and dropped from the count.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kTotalAppsCountHistogramName,
      /*sample=*/6, /*expected_counter=*/1);

  // User signs in and out again one hour later.
  task_environment()->FastForwardBy(base::TimeDelta::FromHours(1));
  ConstructFamilyUserMetricsService();
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  // Should not trigger recording.
  ShutdownFamilyUserMetricsService();

  // No additional metrics were recorded.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kInstalledExtensionsCountHistogramName,
      /*sample=*/3, /*expected_count=*/1);
  // One app has not been used within 28 days and dropped from the count.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::kTotalAppsCountHistogramName,
      /*sample=*/6, /*expected_counter=*/1);
}

INSTANTIATE_TEST_SUITE_P(, FamilyUserAppMetricsTest, testing::Bool());

}  // namespace chromeos
