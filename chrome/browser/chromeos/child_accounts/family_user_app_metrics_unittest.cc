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
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);
constexpr char kStartTime[] = "1 Jan 2020 21:15";
constexpr int kStart = static_cast<int>(apps::mojom::AppType::kUnknown);  // 0
constexpr int kEnd =
    static_cast<int>(apps::mojom::AppType::kSystemWeb);  // max_value

apps::mojom::AppPtr MakeApp(const char* app_id,
                            const char* name,
                            base::Time last_launch_time,
                            apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->name = name;
  app->last_launch_time = last_launch_time;
  app->app_type = app_type;
  return app;
}

}  // namespace

class FamilyUserAppMetricsDerivedForTest : public FamilyUserAppMetrics {
 public:
  explicit FamilyUserAppMetricsDerivedForTest(Profile* profile)
      : FamilyUserAppMetrics(profile) {}
  ~FamilyUserAppMetricsDerivedForTest() override = default;

  void OnNewDay() override { FamilyUserAppMetrics::OnNewDay(); }

  void InitializeAppTypes() {
    for (int app_type = kStart; app_type <= kEnd; app_type++)
      InitializeAppType(static_cast<apps::mojom::AppType>(app_type));
  }

  void InitializeAppType(apps::mojom::AppType app_type) {
    if (!IsAppTypeReady(app_type))
      OnAppTypeInitialized(app_type);
  }
};

// Tests for family user app metrics.
class FamilyUserAppMetricsTest
    : public extensions::ExtensionServiceTestWithInstall,
      public testing::WithParamInterface</*IsFamilyLink=*/bool> {
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

    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.profile_is_supervised = IsFamilyLink();
    InitializeExtensionService(params);

    EXPECT_EQ(IsFamilyLink(), profile()->IsChild());

    supervised_user_service()->Init();
    supervised_user_service()
        ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(true);

    family_user_app_metrics_ =
        std::make_unique<FamilyUserAppMetricsDerivedForTest>(profile());
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
    InstallState expected_state =
        IsFamilyLink() ? INSTALL_WITHOUT_LOAD : INSTALL_NEW;
    const extensions::Extension* extension2 = InstallCRX(path, expected_state);
    ASSERT_TRUE(extension2);
    EXPECT_EQ(IsFamilyLink(),
              registry()->disabled_extensions().Contains(extension2->id()));
    EXPECT_NE(IsFamilyLink(),
              registry()->enabled_extensions().Contains(extension2->id()));
    EXPECT_FALSE(
        extensions::Manifest::IsComponentLocation(extension2->location()));

    // Install an extension, and approve it if the current user is supervised.
    path = data_dir().AppendASCII("good2048.crx");
    const extensions::Extension* extension3 = InstallCRX(path, expected_state);
    ASSERT_TRUE(extension3);
    if (IsFamilyLink()) {
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
    deltas.push_back(MakeApp(/*app_id=*/"u", /*app_name=*/"unknown",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kUnknown));
    deltas.push_back(
        MakeApp(/*app_id=*/"a", /*app_name=*/"arc",
                /*last_launch_time=*/base::Time::Now() - 28 * kOneDay,
                apps::mojom::AppType::kArc));
    deltas.push_back(MakeApp(/*app_id=*/"bu", /*app_name=*/"builtin",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kBuiltIn));
    deltas.push_back(MakeApp(/*app_id=*/"c", /*app_name=*/"crostini",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kCrostini));
    deltas.push_back(MakeApp(/*app_id=*/"e", /*app_name=*/"extension",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kExtension));
    deltas.push_back(MakeApp(/*app_id=*/"w", /*app_name=*/"web",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kWeb));
    deltas.push_back(MakeApp(
        /*app_id=*/"m", /*app_name=*/"macos",
        /*last_launch_time=*/base::Time::Now() - kOneDay,
        apps::mojom::AppType::kMacOs));
    deltas.push_back(MakeApp(
        /*app_id=*/"p", /*app_name=*/"pluginvm",
        /*last_launch_time=*/base::Time::Now() - kOneDay,
        apps::mojom::AppType::kPluginVm));
    deltas.push_back(MakeApp(
        /*app_id=*/"l", /*app_name=*/"lacros",
        /*last_launch_time=*/base::Time::Now() - kOneDay,
        apps::mojom::AppType::kLacros));
    deltas.push_back(MakeApp(
        /*app_id=*/"r", /*app_name=*/"remote",
        /*last_launch_time=*/base::Time::Now() - kOneDay,
        apps::mojom::AppType::kRemote));
    deltas.push_back(MakeApp(/*app_id=*/"bo", /*app_name=*/"borealis",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kBorealis));
    deltas.push_back(MakeApp(/*app_id=*/"s", /*app_name=*/"systemweb",
                             /*last_launch_time=*/base::Time::Now(),
                             apps::mojom::AppType::kSystemWeb));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);

    apps::InstanceRegistry::Instances instances;
    apps::InstanceRegistry& instance_registry =
        apps::AppServiceProxyFactory::GetForProfile(profile())
            ->InstanceRegistry();
    window_ = std::make_unique<aura::Window>(nullptr);
    window_->Init(ui::LAYER_NOT_DRAWN);
    instances.push_back(
        std::make_unique<apps::Instance>(/*app_id=*/"a", window_.get()));
    instance_registry.OnInstances(instances);
  }

  SupervisedUserService* supervised_user_service() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

  bool IsFamilyLink() const { return GetParam(); }

  std::unique_ptr<FamilyUserAppMetricsDerivedForTest> family_user_app_metrics_;
  std::unique_ptr<aura::Window> window_;
};

// Tests the UMA metrics that count the number of installed and enabled
// extensions and themes.
TEST_P(FamilyUserAppMetricsTest, CountInstalledAndEnabledExtensions) {
  base::HistogramTester histogram_tester;

  InstallExtensions();
  family_user_app_metrics_->OnNewDay();

  // There should be 2 installed extensions and one theme.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::GetInstalledExtensionsCountHistogramNameForTest(),
      /*sample=*/3, /*expected_count=*/1);

  if (IsFamilyLink()) {
    // There should be 1 enabled extension and a theme. The other extension
    // lacks parent approval.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetEnabledExtensionsCountHistogramNameForTest(),
        /*sample=*/2, /*expected_count=*/1);
  } else {
    // Regular user case.
    // There should be 2 enabled extensions and a theme.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetEnabledExtensionsCountHistogramNameForTest(),
        /*sample=*/3, /*expected_count=*/1);
  }
}

// Tests the UMA metrics that count the number of recently used apps for
// supervised and regular users.
TEST_P(FamilyUserAppMetricsTest, CountRecentlyUsedApps) {
  base::HistogramTester histogram_tester;

  InstallApps();
  family_user_app_metrics_->OnNewDay();
  family_user_app_metrics_->InitializeAppTypes();

  for (int i = kStart; i <= kEnd; i++) {
    apps::mojom::AppType app_type = static_cast<apps::mojom::AppType>(i);
    const std::string histogram_name =
        FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type);
    histogram_tester.ExpectUniqueSample(histogram_name, /*sample=*/1,
                                        /*expected_count=*/1);
  }
}

// Tests that uninitialized app types are not reported on new day.
TEST_P(FamilyUserAppMetricsTest, UninitializedAppTypeNotReportedOnNewDay) {
  base::HistogramTester histogram_tester;

  InstallApps();
  family_user_app_metrics_->OnNewDay();

  for (int i = kStart; i <= kEnd; i++) {
    apps::mojom::AppType app_type = static_cast<apps::mojom::AppType>(i);
    const std::string histogram_name =
        FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type);
    histogram_tester.ExpectTotalCount(histogram_name, /*expected_count=*/0);
  }
}

// Tests that apps with stale launch dates too far in the past are not counted.
TEST_P(FamilyUserAppMetricsTest, FastForwardOneDay) {
  base::HistogramTester histogram_tester;

  InstallExtensions();
  InstallApps();
  family_user_app_metrics_->InitializeAppTypes();

  // End time is 2 Jan 2020 21:15.
  task_environment()->FastForwardBy(kOneDay);
  family_user_app_metrics_->OnNewDay();

  // One snapshot recorded.
  histogram_tester.ExpectUniqueSample(
      FamilyUserAppMetrics::GetInstalledExtensionsCountHistogramNameForTest(),
      /*sample=*/3, /*expected_count=*/1);
  if (IsFamilyLink()) {
    // There should be 1 enabled extension and a theme.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetEnabledExtensionsCountHistogramNameForTest(),
        /*sample=*/2, /*expected_count=*/1);
  } else {
    // Regular user case.
    // There should be 2 enabled extensions and a theme.
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetEnabledExtensionsCountHistogramNameForTest(),
        /*sample=*/3, /*expected_count=*/1);
  }

  const apps::mojom::AppType fresh_app_types[7] = {
      apps::mojom::AppType::kUnknown,   apps::mojom::AppType::kArc,
      apps::mojom::AppType::kBuiltIn,   apps::mojom::AppType::kCrostini,
      apps::mojom::AppType::kExtension, apps::mojom::AppType::kWeb,
      apps::mojom::AppType::kBorealis,
  };
  // Launched over 28 days ago and dropped from the count.
  const apps::mojom::AppType stale_app_types[4] = {
      apps::mojom::AppType::kMacOs,
      apps::mojom::AppType::kPluginVm,
      apps::mojom::AppType::kLacros,
      apps::mojom::AppType::kRemote,
  };
  for (apps::mojom::AppType app_type : fresh_app_types) {
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type),
        /*sample=*/1,
        /*expected_count=*/1);
  }
  for (apps::mojom::AppType app_type : stale_app_types) {
    histogram_tester.ExpectUniqueSample(
        FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type),
        /*sample=*/0,
        /*expected_count=*/1);
  }
}

// Tests that initializing a single app type only reports metrics for that app
// type, and not other app types.
TEST_P(FamilyUserAppMetricsTest, OnlyReportSingleInitilizedAppTypeOnNewDay) {
  InstallApps();
  family_user_app_metrics_->OnNewDay();

  for (int curr_app_type = kStart; curr_app_type <= kEnd; curr_app_type++) {
    base::HistogramTester histogram_tester;
    // Only report one app type.
    apps::mojom::AppType app_type =
        static_cast<apps::mojom::AppType>(curr_app_type);
    family_user_app_metrics_->InitializeAppType(app_type);
    std::string reported_app_type =
        FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type);
    ASSERT_FALSE(reported_app_type.empty());
    histogram_tester.ExpectUniqueSample(reported_app_type, /*sample=*/1,
                                        /*expected_count=*/1);
    for (int other_app_type = kStart;
         other_app_type <= kEnd && other_app_type != curr_app_type;
         other_app_type++) {
      app_type = static_cast<apps::mojom::AppType>(other_app_type);
      reported_app_type =
          FamilyUserAppMetrics::GetAppsCountHistogramNameForTest(app_type);
      ASSERT_FALSE(reported_app_type.empty());
      histogram_tester.ExpectTotalCount(reported_app_type,
                                        /*expected_count=*/0);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         FamilyUserAppMetricsTest,
                         /*IsFamilyLink=*/testing::Bool());

}  // namespace chromeos
