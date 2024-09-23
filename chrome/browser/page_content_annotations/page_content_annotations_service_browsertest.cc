// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_service.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_enums.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_content_annotations/core/test_page_content_annotator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif

namespace page_content_annotations {

namespace {

using ::testing::UnorderedElementsAre;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Different platforms may execute float models slightly differently, and this
// results in a noticeable difference in the scores. See crbug.com/1307251.
const double kMaxScoreErrorBetweenPlatforms = 0.1;

class TestPageContentAnnotationsObserver
    : public PageContentAnnotationsService::PageContentAnnotationsObserver {
 public:
  void OnPageContentAnnotated(
      const GURL& url,
      const PageContentAnnotationsResult& result) override {
    last_page_content_annotations_result_ = result;
  }
  std::optional<PageContentAnnotationsResult>
      last_page_content_annotations_result_;
};

#endif

}  // namespace

// A HistoryDBTask that retrieves content annotations.
class GetContentAnnotationsTask : public history::HistoryDBTask {
 public:
  GetContentAnnotationsTask(
      const GURL& url,
      base::OnceCallback<void(
          const std::optional<history::VisitContentAnnotations>&)> callback)
      : url_(url), callback_(std::move(callback)) {}
  ~GetContentAnnotationsTask() override = default;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Get visits for URL.
    const history::URLID url_id = db->GetRowForURL(url_, nullptr);
    history::VisitVector visits;
    if (!db->GetVisitsForURL(url_id, &visits)) {
      return true;
    }

    // No visits for URL.
    if (visits.empty()) {
      return true;
    }

    history::VisitContentAnnotations annotations;
    if (db->GetContentAnnotationsForVisit(visits.at(0).visit_id, &annotations))
      stored_content_annotations_ = annotations;

    return true;
  }
  void DoneRunOnMainThread() override {
    std::move(callback_).Run(stored_content_annotations_);
  }

 private:
  // The URL to get content annotations for.
  const GURL url_;
  // The callback to invoke when the database call has completed.
  base::OnceCallback<void(
      const std::optional<history::VisitContentAnnotations>&)>
      callback_;
  // The content annotations that were stored for |url_|.
  std::optional<history::VisitContentAnnotations> stored_content_annotations_;
};

class PageContentAnnotationsServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {optimization_guide::features::kOptimizationHints,
         features::kPageContentAnnotations});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceDisabledBrowserTest,
                       KeyedServiceEnabledButFeaturesDisabled) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

class PageContentAnnotationsServiceKioskModeBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceKioskModeBrowserTest() {
    scoped_feature_list_.InitWithFeatures({features::kPageContentAnnotations},
                                          /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kKioskMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceKioskModeBrowserTest,
                       DisabledInKioskMode) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PageContentAnnotationsServiceEphemeralProfileBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  PageContentAnnotationsServiceEphemeralProfileBrowserTest() {
    scoped_feature_list_.InitWithFeatures({features::kPageContentAnnotations},
                                          /*disabled_features=*/{});
  }

  ~PageContentAnnotationsServiceEphemeralProfileBrowserTest() override {}

  PageContentAnnotationsServiceEphemeralProfileBrowserTest(
      const PageContentAnnotationsServiceEphemeralProfileBrowserTest&) = delete;
  PageContentAnnotationsServiceEphemeralProfileBrowserTest& operator=(
      const PageContentAnnotationsServiceEphemeralProfileBrowserTest&) = delete;

 private:
  ash::GuestSessionMixin guest_session_{&mixin_host_};

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceEphemeralProfileBrowserTest,
                       EphemeralProfileDoesNotInstantiateService) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}
#endif

class PageContentAnnotationsServiceValidationBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceValidationBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kPageContentAnnotationsValidation},
        {features::kPageContentAnnotations});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceValidationBrowserTest,
                       ValidationEnablesService) {
  EXPECT_NE(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

class PageContentAnnotationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentAnnotations,
          {
              {"write_to_history_service", "true"},
          }},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{
            optimization_guide::features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsServiceBrowserTest() override = default;

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void set_load_model_on_startup(bool load_model_on_startup) {
    load_model_on_startup_ = load_model_on_startup;
  }

  PageContentAnnotationsService* service() {
    return PageContentAnnotationsServiceFactory::GetForProfile(
        browser()->profile());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());

    if (load_model_on_startup_) {
      LoadAndWaitForModel();
    }
  }

  void LoadAndWaitForModel(
      std::optional<base::FilePath> specific_model_file_path = std::nullopt) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    base::FilePath model_file_path = specific_model_file_path.value_or(
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("visibility_test_model.tflite"));

    base::HistogramTester histogram_tester;

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetModelForTesting(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
            optimization_guide::TestModelInfoBuilder()
                .SetModelFilePath(model_file_path)
                .Build());

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.ModelExecutor.ModelFileUpdated.PageVisibility", 1);
#else
    base::RunLoop().RunUntilIdle();
#endif
  }

  std::optional<history::VisitContentAnnotations> GetContentAnnotationsForURL(
      const GURL& url) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (!history_service) {
      return std::nullopt;
    }

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    std::optional<history::VisitContentAnnotations> got_content_annotations;

    base::CancelableTaskTracker task_tracker;
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetContentAnnotationsTask>(
            url, base::BindOnce(
                     [](base::RunLoop* run_loop,
                        std::optional<history::VisitContentAnnotations>*
                            out_content_annotations,
                        const std::optional<history::VisitContentAnnotations>&
                            content_annotations) {
                       *out_content_annotations = content_annotations;
                       run_loop->Quit();
                     },
                     run_loop.get(), &got_content_annotations)),
        &task_tracker);

    run_loop->Run();
    return got_content_annotations;
  }

  bool ModelAnnotationsFieldsAreSetForURL(const GURL& url) {
    std::optional<history::VisitContentAnnotations> got_content_annotations =
        GetContentAnnotationsForURL(url);
    // No content annotations -> no model annotations fields.
    if (!got_content_annotations) {
      return false;
    }

    const history::VisitContentModelAnnotations& model_annotations =
        got_content_annotations->model_annotations;

    // Return true if any of the fields have non-empty/non-default values.
    return (model_annotations.visibility_score !=
            history::VisitContentModelAnnotations::kDefaultVisibilityScore) ||
           !model_annotations.categories.empty() ||
           !model_annotations.entities.empty();
  }

  void Annotate(const HistoryVisit& visit) {
    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());
    service->Annotate(visit);
  }

  void WaitForHistoryServiceToFinish() {
    base::RunLoop().RunUntilIdle();
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (!history_service) {
      return;
    }
    history::BlockUntilHistoryProcessesPendingRequests(history_service);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool load_model_on_startup_ = true;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       ContentGetsAnnotatedWhenPageTitleChanges) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url(embedded_test_server()->GetURL("a.test", "/random_title.html"));

  // Navigate to the page for the first time.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Navigate to the page for the second time. This time the page title changes,
  // but the url stays the same.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  int expected_count = 2;
#else
  int expected_count = 0;
#endif

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_TRUE(got_content_annotations->model_annotations.categories.empty());
#endif

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      2);
#else
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", false,
      2);
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  WaitForHistoryServiceToFinish();
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PageContentAnnotations2::kEntryName);
  EXPECT_EQ(2u, entries.size());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       ModelExecutes) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{"Test Page", 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);
#endif

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  int expected_count = 1;
#else
  int expected_count = 0;
#endif
  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      1);
#else
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", false,
      1);
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  WaitForHistoryServiceToFinish();
  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_NE(-1.0, got_content_annotations->model_annotations.visibility_score);
  EXPECT_TRUE(got_content_annotations->model_annotations.categories.empty());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PageContentAnnotations2::kEntryName);
  EXPECT_EQ(1u, entries.size());
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       NonHttpUrlIgnored) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{std::string(), 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);
#endif

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("data:,")));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 0);
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       ENPageVisibilityModel_GoldenData) {
  LoadAndWaitForModel();

  PageContentAnnotationsService* service =
      PageContentAnnotationsServiceFactory::GetForProfile(browser()->profile());

  // Important: Consumers of the visibility score should query the HistoryDB
  // instead of hitting the PCAService directly. We only do this in tests
  // because it is less flaky.
  // TODO(b/258468574): Maybe move this to a navigation-based test once those
  // are less flaky?
  base::RunLoop run_loop;
  service->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].input(), "this is a test");
            EXPECT_EQ(results[0].type(), AnnotationType::kContentVisibility);

            ASSERT_TRUE(results[0].visibility_score());
            EXPECT_NEAR(*results[0].visibility_score(), 0.14453125,
                        kMaxScoreErrorBetweenPlatforms);

            run_loop->Quit();
          },
          &run_loop),
      std::vector<std::string>{"this is a test"},
      AnnotationType::kContentVisibility);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       i18nPageVisibilityModel_GoldenData) {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path =
      source_root_dir.AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("optimization_guide")
          .AppendASCII("i18n_visibility_test_model.tflite");
  LoadAndWaitForModel(model_file_path);

  PageContentAnnotationsService* service =
      PageContentAnnotationsServiceFactory::GetForProfile(browser()->profile());

  // Important: Consumers of the visibility score should query the HistoryDB
  // instead of hitting the PCAService directly. We only do this in tests
  // because it is less flaky.
  // TODO(b/258468574): Maybe move this to a navigation-based test once those
  // are less flaky?
  base::RunLoop run_loop;
  service->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].input(), "google maps");
            EXPECT_EQ(results[0].type(), AnnotationType::kContentVisibility);

            ASSERT_TRUE(results[0].visibility_score());
            EXPECT_NEAR(*results[0].visibility_score(), 0.996094,
                        kMaxScoreErrorBetweenPlatforms);

            run_loop->Quit();
          },
          &run_loop),
      std::vector<std::string>{"google maps"},
      AnnotationType::kContentVisibility);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       NoVisitsForUrlInHistory) {
  HistoryVisit history_visit;
  history_visit.url = GURL("https://probablynotarealurl.com/");
  history_visit.text_to_annotate = "sometext";

  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{"sometext", 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);

  {
    base::HistogramTester histogram_tester;

    Annotate(history_visit);

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
        true, 1);

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus.ModelAnnotations",
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, 1);

    EXPECT_FALSE(GetContentAnnotationsForURL(history_visit.url).has_value());
  }

  {
    base::HistogramTester histogram_tester;

    // Make sure a repeat visit is not annotated again.
    Annotate(history_visit);

    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 0);
  }
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       RegisterPageContentAnnotationsObserver) {
  base::HistogramTester histogram_tester;
  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{"Test Page", 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);

  TestPageContentAnnotationsObserver observer;
  service()->AddObserver(AnnotationType::kContentVisibility, &observer);

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  EXPECT_TRUE(observer.last_page_content_annotations_result_.has_value());
  EXPECT_EQ(AnnotationType::kContentVisibility,
            observer.last_page_content_annotations_result_->GetType());
  EXPECT_NE(-1.0, observer.last_page_content_annotations_result_
                      ->GetContentVisibilityScore());
}

class PageContentAnnotationsServiceRemoteMetadataBrowserTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceRemoteMetadataBrowserTest() {
    // Make sure remote page metadata works without page content annotations
    // enabled.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{page_content_annotations::features::kRemotePageMetadata,
          {{"min_page_category_score", "80"},
           {"supported_countries", "*"},
           {"supported_locales", "*"}}}},
        /*disabled_features=*/{{features::kPageContentAnnotations}});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceRemoteMetadataBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresAllTheThingsFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::PageEntitiesMetadata page_entities_metadata;
  optimization_guide::proto::Entity* entity =
      page_entities_metadata.add_entities();
  entity->set_entity_id("entity1");
  entity->set_score(50);
  optimization_guide::proto::Category* category =
      page_entities_metadata.add_categories();
  category->set_category_id("category1");
  category->set_score(0.85);
  optimization_guide::proto::Category* category2 =
      page_entities_metadata.add_categories();
  category2->set_category_id("othercategory");
  category2->set_score(0.75);
  page_entities_metadata.set_alternative_title("alternative title");
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::PAGE_ENTITIES,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitForHistoryServiceToFinish();

  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_THAT(
      got_content_annotations->model_annotations.entities,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
  EXPECT_THAT(
      got_content_annotations->model_annotations.categories,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("category1", 85)));
  EXPECT_EQ(got_content_annotations->alternative_title, "alternative title");
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresPageEntitiesAndCategoriesFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::PageEntitiesMetadata page_entities_metadata;
  optimization_guide::proto::Entity* entity =
      page_entities_metadata.add_entities();
  entity->set_entity_id("entity1");
  entity->set_score(50);
  optimization_guide::proto::Category* category =
      page_entities_metadata.add_categories();
  category->set_category_id("category1");
  category->set_score(0.85);
  optimization_guide::proto::Category* category2 =
      page_entities_metadata.add_categories();
  category2->set_category_id("othercategory");
  category2->set_score(0.75);
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::PAGE_ENTITIES,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitForHistoryServiceToFinish();

  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_THAT(
      got_content_annotations->model_annotations.entities,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
  EXPECT_THAT(
      got_content_annotations->model_annotations.categories,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("category1", 85)));
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresAlternateTitleFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::PageEntitiesMetadata page_entities_metadata;
  page_entities_metadata.set_alternative_title("alternative title");
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::PAGE_ENTITIES,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitForHistoryServiceToFinish();

  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_EQ(got_content_annotations->alternative_title, "alternative title");
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       EmptyMetadataNotStored) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::PageEntitiesMetadata page_entities_metadata;
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::PAGE_ENTITIES,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();

  history::VisitContentAnnotations got_content_annotations =
      GetContentAnnotationsForURL(url).value_or(
          history::VisitContentAnnotations());
  EXPECT_TRUE(got_content_annotations.alternative_title.empty());
}

class PageContentAnnotationsServiceSalientImageMetadataBrowserTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceSalientImageMetadataBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentAnnotations, {}},
         {features::kPageContentAnnotationsPersistSalientImageMetadata,
          {{"supported_countries", "*"}, {"supported_locales", "*"}}}},
        /*disabled_features=*/{});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceSalientImageMetadataBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PageContentAnnotationsServiceSalientImageMetadataBrowserTest,
    EmptyMetadataNotStored) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::SalientImageMetadata salient_image_metadata;
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(salient_image_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::SALIENT_IMAGE,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();

  history::VisitContentAnnotations got_content_annotations =
      GetContentAnnotationsForURL(url).value_or(
          history::VisitContentAnnotations());
  ASSERT_FALSE(got_content_annotations.has_url_keyed_image);
}

IN_PROC_BROWSER_TEST_F(
    PageContentAnnotationsServiceSalientImageMetadataBrowserTest,
    MetadataWithNoNonEmptyUrlNotStored) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::SalientImageMetadata salient_image_metadata;
  salient_image_metadata.add_thumbnails();
  salient_image_metadata.add_thumbnails();
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(salient_image_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::SALIENT_IMAGE,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();

  history::VisitContentAnnotations got_content_annotations =
      GetContentAnnotationsForURL(url).value_or(
          history::VisitContentAnnotations());
  ASSERT_FALSE(got_content_annotations.has_url_keyed_image);
}

IN_PROC_BROWSER_TEST_F(
    PageContentAnnotationsServiceSalientImageMetadataBrowserTest,
    MetadataWithNonEmptyUrlStored) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));

  optimization_guide::proto::SalientImageMetadata salient_image_metadata;
  salient_image_metadata.add_thumbnails();
  salient_image_metadata.add_thumbnails()->set_image_url(
      "http://gstatic.com/image");
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(salient_image_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, optimization_guide::proto::SALIENT_IMAGE,
                          metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  WaitForHistoryServiceToFinish();

  std::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_TRUE(got_content_annotations->has_url_keyed_image);
}

class PageContentAnnotationsServiceNoHistoryTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceNoHistoryTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentAnnotations,
          {
              {"write_to_history_service", "false"},
          }},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceNoHistoryTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceNoHistoryTest,
                       ModelExecutesButDoesntWriteToHistory) {
  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{"Test Page", 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);

  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      1);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceNoHistoryTest,
                       ModelExecutesAndUsesCachedResult) {
  TestPageContentAnnotator test_annotator;
  test_annotator.UseVisibilityScores(std::nullopt, {{"Test Page", 0.5}});
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);

  {
    base::HistogramTester histogram_tester;

    GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        false, 1);
  }
  {
    base::HistogramTester histogram_tester;
    GURL url2(embedded_test_server()->GetURL("a.test", "/hello.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
        true, 1);

    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        true, 1);
  }
}

class PageContentAnnotationsServiceBatchVisitTest
    : public PageContentAnnotationsServiceNoHistoryTest {
 public:
  PageContentAnnotationsServiceBatchVisitTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentAnnotations,
          {{"write_to_history_service", "false"},
           {"annotate_visit_batch_size", "2"}}},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceBatchVisitTest() override = default;

  void SetUpOnMainThread() override {
    PageContentAnnotationsServiceNoHistoryTest::SetUpOnMainThread();

    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());

    annotator_.UseVisibilityScores(
        /*model_info=*/std::nullopt, {
                                         {
                                             "Test Page",
                                             0.5,
                                         },
                                         {
                                             "sometext",
                                             0.7,
                                         },
                                     });
    service->OverridePageContentAnnotatorForTesting(&annotator_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestPageContentAnnotator annotator_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitTest,
                       ModelExecutesWithFullBatch) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.AnnotationRequested", 1);

  GURL url2(embedded_test_server()->GetURL("b.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 2);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      2);

  base::RunLoop().RunUntilIdle();

  // The cache is missed because we are batching requests. The cache check
  // happens before adding a visit annotation request to the batch.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false, 2);
  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

class PageContentAnnotationsServiceBatchVisitNoAnnotateTest
    : public PageContentAnnotationsServiceBatchVisitTest {
 public:
  PageContentAnnotationsServiceBatchVisitNoAnnotateTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageContentAnnotations,
          {{"write_to_history_service", "false"},
           {"annotate_visit_batch_size", "1"}}},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceBatchVisitNoAnnotateTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitNoAnnotateTest,
                       QueueFullAndVisitBatchActive) {
  TestPageContentAnnotator test_annotator;
  test_annotator.SetAlwaysHang(true);
  service()->OverridePageContentAnnotatorForTesting(&test_annotator);

  base::HistogramTester histogram_tester;
  HistoryVisit history_visit1(base::Time::Now(),
                              GURL("https://probablynotarealurl1.com/"));
  HistoryVisit history_visit2(base::Time::Now(),
                              GURL("https://probablynotarealurl2.com/"));
  HistoryVisit history_visit3(base::Time::Now(),
                              GURL("https://probablynotarealurl3.com/"));
  history_visit1.text_to_annotate = "sometext1";
  history_visit2.text_to_annotate = "sometext2";
  history_visit3.text_to_annotate = "sometext3";

  Annotate(history_visit1);
  Annotate(history_visit2);
  Annotate(history_visit3);

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", 1);

  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", true, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false, 3);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitTest,
                       NoModelExecutionWithoutFullBatch) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.test", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.AnnotationRequestQueued", 1);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", 0);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace page_content_annotations
