// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/arc_task_window_builder.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_process_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/arc/tracing/test/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/ash/extensions/autotest_private/autotest_private_api.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/device_event_log/device_event_log.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "services/viz/privileged/mojom/compositing/features.mojom-features.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::Return;

namespace extensions {

namespace {

class TestSearchProvider : public app_list::SearchProvider {
 public:
  explicit TestSearchProvider(ash::AppListSearchResultType result_type)
      : SearchProvider(app_list::SearchCategory::kTest),
        result_type_(result_type) {}

  ~TestSearchProvider() override = default;

  void SetNextResults(
      std::vector<std::unique_ptr<ChromeSearchResult>> results) {
    results_ = std::move(results);
  }

  ash::AppListSearchResultType ResultType() const override {
    return result_type_;
  }

  void Start(const std::u16string& query) override {
    DCHECK(!ash::IsZeroStateResultType(result_type_));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TestSearchProvider::SetResults,
                                  query_weak_factory_.GetWeakPtr()));
  }

  void StopQuery() override { query_weak_factory_.InvalidateWeakPtrs(); }

  void StartZeroState() override {}

 private:
  void SetResults() { SwapResults(&results_); }

  std::vector<std::unique_ptr<ChromeSearchResult>> results_;
  ash::AppListSearchResultType result_type_;
  base::WeakPtrFactory<TestSearchProvider> query_weak_factory_{this};
};

}  // namespace

class AutotestPrivateApiTest : public ExtensionApiTest {
 public:
  AutotestPrivateApiTest() {
    feature_list_.InitAndEnableFeature(viz::mojom::EnableVizTestApis);
  }

  AutotestPrivateApiTest(const AutotestPrivateApiTest&) = delete;
  AutotestPrivateApiTest& operator=(const AutotestPrivateApiTest&) = delete;

  ~AutotestPrivateApiTest() override = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Make ARC enabled for tests.
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Turn on testing mode so we don't kill the browser.
    AutotestPrivateAPI::GetFactoryInstance()
        ->Get(browser()->profile())
        ->set_test_mode(true);
  }

  bool RunAutotestPrivateExtensionTest(
      const std::string& test_suite,
      base::Value::List suite_args = base::Value::List()) {
    base::Value::Dict custom_args;
    custom_args.Set("testSuite", test_suite);
    custom_args.Set("args", std::move(suite_args));

    std::string json;
    if (!base::JSONWriter::Write(custom_args, &json)) {
      LOG(ERROR) << "Failed to parse custom args into json.";
      return false;
    }

    return RunExtensionTest("autotest_private", {.custom_arg = json.c_str()},
                            {.load_as_component = true});
  }

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/356369542): Fix flakiness on sanitizer bots.

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_AutotestPrivate DISABLED_AutotestPrivate
#else
#define MAYBE_AutotestPrivate AutotestPrivate
#endif
IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, MAYBE_AutotestPrivate) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("default")) << message_;
}

// Set of tests where ARC is enabled and test apps and packages are registered.
// TODO(crbug.com/41486987): re-enable the following test.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_AutotestPrivateArcEnabled DISABLED_AutotestPrivateArcEnabled
#else
#define MAYBE_AutotestPrivateArcEnabled AutotestPrivateArcEnabled
#endif
IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest,
                       MAYBE_AutotestPrivateArcEnabled) {
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);

  arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
      std::make_unique<arc::ArcSessionRunner>(
          base::BindRepeating(arc::FakeArcSession::Create)));

  // Having ARC Terms accepted automatically bypasses TOS stage.
  // Set it before |arc::SetArcPlayStoreEnabledForProfile|
  browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcTermsAccepted,
                                               true);
  arc::SetArcPlayStoreEnabledForProfile(profile(), true);
  // Provisioning is completed.
  browser()->profile()->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  // Start ARC
  arc::ArcSessionManager::Get()->StartArcForTesting();

  auto app_instance = std::make_unique<arc::FakeAppInstance>(prefs);
  prefs->app_connection_holder()->SetInstance(app_instance.get());
  arc::WaitForInstanceReady(prefs->app_connection_holder());

  std::vector<arc::mojom::AppInfoPtr> fake_apps;
  fake_apps.emplace_back(arc::mojom::AppInfo::New("Fake App", "fake.package",
                                                  "fake.package.activity"));
  app_instance->SendRefreshAppList(fake_apps);

  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.emplace_back(arc::mojom::ArcPackageInfo::New(
      fake_apps[0]->package_name, 10 /* package_version */,
      100 /* last_backup_android_id */,
      base::Time::Now()
          .ToDeltaSinceWindowsEpoch()
          .InMicroseconds() /* last_backup_time */,
      true /* sync */));
  app_instance->SendRefreshPackageList(std::move(packages));

  arc::FakeProcessInstance fake_process_instance;
  arc::ArcServiceManager::Get()->arc_bridge_service()->process()->SetInstance(
      &fake_process_instance);
  fake_process_instance.set_request_low_memory_kill_counts_response(
      arc::mojom::LowMemoryKillCounts::New(1,    // oom.
                                           2,    // lmkd_foreground.
                                           3,    // lmkd_perceptible.
                                           4,    // lmkd_cached.
                                           5,    // pressure_foreground.
                                           6,    // pressure_perceptible.
                                           7));  // pressure_cached.

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("arcEnabled")) << message_;

  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
}

// TODO(crbug.com/331532893): Flaky on ASan/LSan.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_AutotestPrivateArcProcess DISABLED_AutotestPrivateArcProcess
#else
#define MAYBE_AutotestPrivateArcProcess AutotestPrivateArcProcess
#endif
IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest,
                       MAYBE_AutotestPrivateArcProcess) {
  arc::FakeProcessInstance fake_process_instance;
  arc::ArcServiceManager::Get()->arc_bridge_service()->process()->SetInstance(
      &fake_process_instance);
  fake_process_instance.set_request_low_memory_kill_counts_response(
      arc::mojom::LowMemoryKillCounts::New(1,    // oom.
                                           2,    // lmkd_foreground.
                                           3,    // lmkd_perceptible.
                                           4,    // lmkd_cached.
                                           5,    // pressure_foreground.
                                           6,    // pressure_perceptible.
                                           7));  // pressure_cached.

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("arcProcess")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, ScrollableShelfAPITest) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("scrollableShelf")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, ShelfAPITest) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("shelf")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, IsFeatureEnabled) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("isFeatureEnabled")) << message_;
}

class AutotestPrivateHoldingSpaceApiTest
    : public AutotestPrivateApiTest,
      public ::testing::WithParamInterface<bool /* mark_time_of_first_add */> {
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutotestPrivateHoldingSpaceApiTest,
                         ::testing::Bool() /* mark_time_of_first_add */);

IN_PROC_BROWSER_TEST_P(AutotestPrivateHoldingSpaceApiTest,
                       HoldingSpaceAPITest) {
  auto* prefs = browser()->profile()->GetPrefs();

  ash::holding_space_prefs::SetPreviewsEnabled(prefs, false);
  ash::holding_space_prefs::MarkTimeOfFirstAdd(prefs);
  ash::holding_space_prefs::MarkTimeOfFirstAvailability(prefs);
  ash::holding_space_prefs::MarkTimeOfFirstEntry(prefs);
  ash::holding_space_prefs::MarkTimeOfFirstFilesAppChipPress(prefs);
  ash::holding_space_prefs::MarkTimeOfFirstPin(prefs);

  const bool mark_time_of_first_add = GetParam();

  base::Value::Dict options;
  options.Set("markTimeOfFirstAdd", mark_time_of_first_add);
  base::Value::List suite_args;
  suite_args.Append(std::move(options));

  ASSERT_TRUE(
      RunAutotestPrivateExtensionTest("holdingSpace", std::move(suite_args)))
      << message_;

  std::optional<base::Time> timeOfFirstAdd =
      ash::holding_space_prefs::GetTimeOfFirstAdd(prefs);
  std::optional<base::Time> timeOfFirstAvailability =
      ash::holding_space_prefs::GetTimeOfFirstAvailability(prefs);

  ASSERT_TRUE(ash::holding_space_prefs::IsPreviewsEnabled(prefs));
  ASSERT_EQ(timeOfFirstAdd.has_value(), mark_time_of_first_add);
  ASSERT_NE(timeOfFirstAvailability, std::nullopt);
  ASSERT_EQ(ash::holding_space_prefs::GetTimeOfFirstEntry(prefs), std::nullopt);
  ASSERT_EQ(ash::holding_space_prefs::GetTimeOfFirstFilesAppChipPress(prefs),
            std::nullopt);
  ASSERT_EQ(ash::holding_space_prefs::GetTimeOfFirstPin(prefs), std::nullopt);

  if (timeOfFirstAdd) {
    ASSERT_GT(timeOfFirstAdd, timeOfFirstAvailability);
  }
}

class AutotestPrivateApiOverviewTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateApiOverviewTest() = default;

  // AutotestPrivateApiTest:
  void SetUpOnMainThread() override {
    AutotestPrivateApiTest::SetUpOnMainThread();

    // Create one additional browser window to make total of 2 windows.
    CreateBrowser(browser()->profile());

    // Enters tablet overview mode.
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    base::RunLoop run_loop;
    ash::OverviewTestApi().SetOverviewMode(
        /*start=*/true, base::BindLambdaForTesting([&run_loop](bool finished) {
          if (!finished) {
            ADD_FAILURE() << "Failed to enter overview.";
          }
          run_loop.Quit();
        }));
    run_loop.Run();

    // We should get 2 overview items from the 2 browser windows.
    ASSERT_EQ(2u, ash::OverviewTestApi().GetOverviewInfo()->size());
  }

  gfx::NativeWindow GetRootWindow() const {
    return browser()->window()->GetNativeWindow()->GetRootWindow();
  }
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiOverviewTest, Default) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("overviewDefault")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiOverviewTest, Drag) {
  const ash::OverviewInfo info =
      ash::OverviewTestApi().GetOverviewInfo().value();
  const gfx::Point start_point =
      info.begin()->second.bounds_in_screen.CenterPoint();

  // Long press to pick up an overview item and drag it a bit.
  ui::test::EventGenerator generator(GetRootWindow());

  generator.set_current_screen_location(start_point);
  generator.PressTouch();

  ui::GestureEvent long_press(
      start_point.x(), start_point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator.Dispatch(&long_press);

  // 50 is arbitrary number of dip to move a bit to ensure the item is being
  // dragged.
  const gfx::Point end_point(start_point.x() + 50, start_point.y());
  generator.MoveTouch(end_point);

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("overviewDrag")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiOverviewTest, PrimarySnapped) {
  const ash::OverviewInfo info =
      ash::OverviewTestApi().GetOverviewInfo().value();
  const gfx::Point start_point =
      info.begin()->second.bounds_in_screen.CenterPoint();
  const gfx::Point end_point(0, start_point.y());

  // Long press to pick up an overview item, drag all the way to the left
  // to snap it on left.
  ui::test::EventGenerator generator(GetRootWindow());

  generator.set_current_screen_location(start_point);
  generator.PressTouch();

  ui::GestureEvent long_press(
      start_point.x(), start_point.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  generator.Dispatch(&long_press);

  generator.MoveTouch(end_point);
  generator.ReleaseTouch();

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("splitviewPrimarySnapped"))
      << message_;
}

class AutotestPrivateWithPolicyApiTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateWithPolicyApiTest() {}

  AutotestPrivateWithPolicyApiTest(const AutotestPrivateWithPolicyApiTest&) =
      delete;
  AutotestPrivateWithPolicyApiTest& operator=(
      const AutotestPrivateWithPolicyApiTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    AutotestPrivateApiTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    AutotestPrivateApiTest::SetUpOnMainThread();
    // Set a fake policy
    policy::PolicyMap policy;
    policy.Set(policy::key::kAllowDinosaurEasterEgg,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// GetAllEnterprisePolicies Sanity check.
IN_PROC_BROWSER_TEST_F(AutotestPrivateWithPolicyApiTest, PolicyAPITest) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("enterprisePolicies"))
      << message_;
}

class AutotestPrivateArcPerformanceTracing : public AutotestPrivateApiTest {
 public:
  AutotestPrivateArcPerformanceTracing() = default;

  AutotestPrivateArcPerformanceTracing(
      const AutotestPrivateArcPerformanceTracing&) = delete;
  AutotestPrivateArcPerformanceTracing& operator=(
      const AutotestPrivateArcPerformanceTracing&) = delete;

  ~AutotestPrivateArcPerformanceTracing() override = default;

 protected:
  // AutotestPrivateApiTest:
  void SetUpOnMainThread() override {
    AutotestPrivateApiTest::SetUpOnMainThread();

    tracing_helper_.SetUp(profile());
    root_surface_ = std::make_unique<exo::Surface>();
    performance_tracing()->SetCustomSessionReadyCallbackForTesting(
        base::BindRepeating(
            &arc::ArcAppPerformanceTracingTestHelper::PlayDefaultSequence,
            base::Unretained(&tracing_helper_), root_surface_.get()));
  }

  void TearDownOnMainThread() override {
    root_surface_.reset();
    performance_tracing()->SetCustomSessionReadyCallbackForTesting(
        arc::ArcAppPerformanceTracing::CustomSessionReadyCallback());
    tracing_helper_.TearDown();

    AutotestPrivateApiTest::TearDownOnMainThread();
  }

  arc::ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

  arc::ArcAppPerformanceTracing* performance_tracing() {
    return tracing_helper_.GetTracing();
  }

  std::unique_ptr<exo::Surface> root_surface_;

 private:
  arc::ArcAppPerformanceTracingTestHelper tracing_helper_;
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateArcPerformanceTracing, Basic) {
  const auto arc_widget = arc::ArcTaskWindowBuilder()
                              .SetShellRootSurface(root_surface_.get())
                              .BuildOwnsNativeWidget();

  performance_tracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), nullptr);

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("arcPerformanceTracing"))
      << message_;

  performance_tracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      nullptr, arc_widget->GetNativeWindow());
}

class AutotestPrivateSystemWebAppsTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateSystemWebAppsTest() {
    installation_ =
        ash::TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
  ~AutotestPrivateSystemWebAppsTest() override = default;

 private:
  std::unique_ptr<ash::TestSystemWebAppInstallation> installation_;
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateSystemWebAppsTest, SystemWebApps) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("systemWebApps")) << message_;
}

class AutotestPrivateLacrosTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateLacrosTest(const AutotestPrivateLacrosTest&) = delete;
  AutotestPrivateLacrosTest& operator=(const AutotestPrivateLacrosTest&) =
      delete;

 protected:
  AutotestPrivateLacrosTest() {
    auto enabled_features = ash::standalone_browser::GetFeatureRefs();
    enabled_features.push_back(viz::mojom::EnableVizTestApis);
    feature_list_.InitWithFeatures(
        enabled_features,
        // Disable ash extension keeplist so that the test extension will not
        // be blocked in Ash.
        {ash::features::kEnforceAshExtensionKeeplist});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);

    crosapi::BrowserManager::DisableForTesting();
  }
  ~AutotestPrivateLacrosTest() override {
    crosapi::BrowserManager::EnableForTesting();
  }

  void SetUpOnMainThread() override {
    // For testing APIs, we need web browser instance as JS runtime.
    Browser::CreateParams params(ProfileManager::GetLastUsedProfile(), false);
    Browser::Create(params);
    SelectFirstBrowser();

    AutotestPrivateApiTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateLacrosTest, Lacros) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("lacrosEnabled")) << message_;
}

class AutotestPrivateSearchTest
    : public AutotestPrivateApiTest,
      public ::testing::WithParamInterface</* tablet_mode =*/bool> {
 public:
  AutotestPrivateSearchTest() = default;

  ~AutotestPrivateSearchTest() override = default;
  AutotestPrivateSearchTest(const AutotestPrivateSearchTest&) = delete;
  AutotestPrivateSearchTest& operator=(const AutotestPrivateSearchTest&) =
      delete;

  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
  PublishedResults() {
    return AppListClientImpl::GetInstance()
        ->GetModelUpdaterForTest()
        ->GetPublishedSearchResultsForTest();
  }

  void SetUpSearchResults() {
    auto search_provider_ = std::make_unique<TestSearchProvider>(
        ash::AppListSearchResultType::kOmnibox);
    search_provider_->SetNextResults(
        MakeResults({"youtube"}, {ash::SearchResultDisplayType::kList},
                    {ash::AppListSearchResultCategory::kWeb}, {1}, {0.8}));

    app_list::SearchController* search_controller =
        AppListClientImpl::GetInstance()->search_controller();
    EXPECT_EQ(1u, search_controller->ReplaceProvidersForResultTypeForTest(
                      ash::AppListSearchResultType::kOmnibox,
                      std::move(search_provider_)));
  }

 protected:
  std::vector<std::unique_ptr<ChromeSearchResult>> MakeResults(
      const std::vector<std::string>& ids,
      const std::vector<ash::SearchResultDisplayType>& display_types,
      const std::vector<ash::AppListSearchResultCategory>& categories,
      const std::vector<int>& best_match_ranks,
      const std::vector<double>& scores) {
    std::vector<std::unique_ptr<ChromeSearchResult>> results;
    for (size_t i = 0; i < ids.size(); ++i) {
      std::unique_ptr<app_list::TestResult> test_result =
          std::make_unique<app_list::TestResult>(
              ids[i], display_types[i], categories[i], best_match_ranks[i],
              /*relevance=*/scores[i], /*ftrl_result_score=*/scores[i]);
      test_result->scoring().override_filter_for_test(true);
      results.emplace_back(std::move(test_result));
    }
    return results;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AutotestPrivateSearchTest,
                         /* tablet_mode= */ ::testing::Bool());

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LauncherSearchBoxStateAPITest \
  DISABLED_LauncherSearchBoxStateAPITest
#else
#define MAYBE_LauncherSearchBoxStateAPITest LauncherSearchBoxStateAPITest
#endif
IN_PROC_BROWSER_TEST_P(AutotestPrivateSearchTest,
                       MAYBE_LauncherSearchBoxStateAPITest) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(GetParam());
  test::GetAppListClient()->ShowAppList(ash::AppListShowSource::kSearchKey);
  if (!GetParam()) {
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
  }

  ui::test::EventGenerator generator(
      browser()->window()->GetNativeWindow()->GetRootWindow());
  generator.GestureTapAt(
      ash::GetSearchBoxView()->GetBoundsInScreen().CenterPoint());

  app_list::SearchResultsChangedWaiter results_changed_waiter(
      AppListClientImpl::GetInstance()->search_controller(),
      {app_list::ResultType::kOmnibox});
  app_list::ResultsWaiter results_waiter(u"outube");

  SetUpSearchResults();
  ash::AppListTestApi().SimulateSearch(u"outube");

  results_changed_waiter.Wait();
  results_waiter.Wait();

  std::vector<ChromeSearchResult*> results;
  for (ChromeSearchResult* result : PublishedResults()) {
    // There may be zero state results that are also published, but not visible
    // in the UI. This test should only check search list results.
    if (result->display_type() != ash::SearchResultDisplayType::kList) {
      continue;
    }

    results.push_back(result);
  }

  ASSERT_EQ(results.size(), 1u);
  ASSERT_TRUE(results[0]);
  EXPECT_EQ(base::UTF16ToASCII(results[0]->title()), "youtube");

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("launcherSearchBoxState"))
      << message_;
}

class AutotestPrivateIsFieldTrialActiveApiTest : public AutotestPrivateApiTest {
 public:
  AutotestPrivateIsFieldTrialActiveApiTest() {
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        "ActiveTrialForTest", "GroupForTest");
    trial->Activate();
  }
};

IN_PROC_BROWSER_TEST_F(AutotestPrivateIsFieldTrialActiveApiTest,
                       IsFieldTrialActive) {
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("isFieldTrialActive"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, ClearAllowedPref) {
  static constexpr auto kTestTheme =
      ash::personalization_app::mojom::AmbientTheme::kFloatOnBy;
  ash::personalization_app::mojom::AmbientTheme default_theme =
      ash::AmbientUiSettings::ReadFromPrefService(
          *browser()->profile()->GetPrefs())
          .theme();
  ASSERT_NE(kTestTheme, default_theme);
  ash::AmbientUiSettings(kTestTheme)
      .WriteToPrefService(*browser()->profile()->GetPrefs());

  base::Value::List suite_args;
  suite_args.Append(base::Value(ash::ambient::prefs::kAmbientUiSettings));

  ASSERT_TRUE(RunAutotestPrivateExtensionTest("clearAllowedPref",
                                              std::move(suite_args)))
      << message_;
  // Value read back should be the default.
  EXPECT_EQ(ash::AmbientUiSettings::ReadFromPrefService(
                *browser()->profile()->GetPrefs())
                .theme(),
            default_theme);
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, SetDeviceLanguage) {
  std::string target_locale = "ja-JP";
  base::Value::List args;
  args.Append(base::Value(target_locale));
  ASSERT_TRUE(
      RunAutotestPrivateExtensionTest("setDeviceLanguage", std::move(args)))
      << message_;
  std::string cur_locale = browser()->profile()->GetPrefs()->GetString(
      language::prefs::kApplicationLocale);
  EXPECT_EQ(cur_locale, target_locale);
}

IN_PROC_BROWSER_TEST_F(AutotestPrivateApiTest, GetDeviceEventLog) {
  device_event_log::ClearAll();
  PRINTER_LOG(DEBUG) << "PrinterTestLog";
  NET_LOG(DEBUG) << "NetworkTestLog";
  USB_LOG(DEBUG) << "USBTestLog";
  ASSERT_TRUE(RunAutotestPrivateExtensionTest("getDeviceEventLog")) << message_;
}

}  // namespace extensions
