// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace optimization_guide {

namespace {

using ::testing::UnorderedElementsAre;

}  // namespace

class FakePageTextService : public mojom::PageTextService {
 public:
  FakePageTextService() = default;
  ~FakePageTextService() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    // Reset first in case the pipe is being re-used, as for a second navigation
    // in a test.
    receiver_.reset();

    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PageTextService>(
        std::move(handle)));
  }

  // mojom::PageTextService:
  void RequestPageTextDump(
      mojom::PageTextDumpRequestPtr request,
      mojo::PendingRemote<mojom::PageTextConsumer> consumer) override {
    mojo::Remote<mojom::PageTextConsumer> consumer_remote;
    consumer_remote.Bind(std::move(consumer));

    consumer_remote->OnTextDumpChunk(u"hello world");
    consumer_remote->OnChunksEnd();
  }

 private:
  mojo::AssociatedReceiver<mojom::PageTextService> receiver_{this};
};

// A HistoryDBTask that retrieves content annotations.
class GetContentAnnotationsTask : public history::HistoryDBTask {
 public:
  GetContentAnnotationsTask(
      const GURL& url,
      base::OnceCallback<void(
          const absl::optional<history::VisitContentAnnotations>&)> callback)
      : url_(url), callback_(std::move(callback)) {}
  ~GetContentAnnotationsTask() override = default;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Get visits for URL.
    const history::URLID url_id = db->GetRowForURL(url_, nullptr);
    history::VisitVector visits;
    if (!db->GetVisitsForURL(url_id, &visits))
      return true;

    // No visits for URL.
    if (visits.empty())
      return true;

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
      const absl::optional<history::VisitContentAnnotations>&)>
      callback_;
  // The content annotations that were stored for |url_|.
  absl::optional<history::VisitContentAnnotations> stored_content_annotations_;
};

class PageContentAnnotationsServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {features::kOptimizationHints, features::kPageContentAnnotations});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceDisabledBrowserTest,
                       KeyedServiceEnabledButFeaturesDisabled) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

class PageContentAnnotationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "true"},
          }}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceBrowserTest() override = default;

  void set_load_model_on_startup(bool load_model_on_startup) {
    load_model_on_startup_ = load_model_on_startup;
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());

    InstallFakePageTextAgent();

    if (load_model_on_startup_) {
      LoadAndWaitForModel();
    }
  }

  // TODO(crbug/1256940): Fix the root cause and remove this gross workaround.
  void InstallFakePageTextAgent() {
    fake_renderer_service_ = std::make_unique<FakePageTextService>();

    blink::AssociatedInterfaceProvider* remote_interfaces =
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetMainFrame()
            ->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PageTextService::Name_,
        base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                            base::Unretained(fake_renderer_service_.get())));
  }

  void LoadAndWaitForModel() {
    proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    proto::PageTopicsModelMetadata page_topics_model_metadata;
    page_topics_model_metadata.set_version(123);
    page_topics_model_metadata.add_supported_output(
        proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
    auto* output_params =
        page_topics_model_metadata.mutable_output_postprocessing_params();
    auto* category_params = output_params->mutable_category_params();
    category_params->set_max_categories(5);
    category_params->set_min_none_weight(0.8);
    category_params->set_min_category_weight(0.0);
    category_params->set_min_normalized_weight_within_top_n(0.1);
    // TODO(crbug.com/1200677): migrate the category name on the test model
    // itself provided by model owners.
    output_params->mutable_visibility_params()->set_category_name(
        "FLOC_PROTECTED");
    page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("bert_page_topics_model.tflite");

    base::HistogramTester histogram_tester;

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetModelForTesting(
            proto::OPTIMIZATION_TARGET_PAGE_TOPICS,
            optimization_guide::TestModelInfoBuilder()
                .SetModelFilePath(model_file_path)
                .SetModelMetadata(any_metadata)
                .Build());

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.ModelExecutor.ModelFileUpdated.PageTopics", 1);
#else
    base::RunLoop().RunUntilIdle();
#endif
  }

  absl::optional<history::VisitContentAnnotations> GetContentAnnotationsForURL(
      const GURL& url) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (!history_service)
      return absl::nullopt;

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    absl::optional<history::VisitContentAnnotations> got_content_annotations;

    base::CancelableTaskTracker task_tracker;
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetContentAnnotationsTask>(
            url, base::BindOnce(
                     [](base::RunLoop* run_loop,
                        absl::optional<history::VisitContentAnnotations>*
                            out_content_annotations,
                        const absl::optional<history::VisitContentAnnotations>&
                            content_annotations) {
                       *out_content_annotations = content_annotations;
                       run_loop->Quit();
                     },
                     run_loop.get(), &got_content_annotations)),
        &task_tracker);

    run_loop->Run();
    return got_content_annotations;
  }

  void Annotate(const HistoryVisit& visit, const std::string& text) {
    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());
    service->Annotate(visit, text);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakePageTextService> fake_renderer_service_;
  bool load_model_on_startup_ = true;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       ModelExecutes) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  int expected_count = 1;
#else
  int expected_count = 0;
#endif
  RetryForHistogramUntilCountReached(
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
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_NE(-1.0, got_content_annotations->model_annotations.visibility_score);
  EXPECT_FALSE(got_content_annotations->model_annotations.categories.empty());
  EXPECT_EQ(
      123,
      got_content_annotations->model_annotations.page_topics_model_version);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::PageContentAnnotations::kEntryName);
  EXPECT_EQ(1u, entries.size());

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       NoVisitsForUrlInHistory) {
  HistoryVisit history_visit;
  history_visit.url = GURL("https://probablynotarealurl.com/");

  {
    base::HistogramTester histogram_tester;

    Annotate(history_visit, "sometext");

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
        true, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, 1);

    EXPECT_FALSE(GetContentAnnotationsForURL(history_visit.url).has_value());
  }

  {
    base::HistogramTester histogram_tester;

    // Make sure a repeat visit is not annotated again.
    Annotate(history_visit, "sometext");

    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 0);
  }
}

class PageContentAnnotationsServiceRemotePageEntitiesBrowserTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceRemotePageEntitiesBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "true"},
              {"fetch_remote_page_entities", "true"},
          }}},
        /*disabled_features=*/{});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceRemotePageEntitiesBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PageContentAnnotationsServiceRemotePageEntitiesBrowserTest,
    StoresPageEntitiesFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));

  proto::PageEntitiesMetadata page_entities_metadata;
  proto::Entity* entity = page_entities_metadata.add_entities();
  entity->set_entity_id("entity1");
  entity->set_score(50);
  OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, proto::PAGE_ENTITIES, metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_THAT(
      got_content_annotations->model_annotations.entities,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
}

class PageContentAnnotationsServiceNoHistoryTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceNoHistoryTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "false"},
          }}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceNoHistoryTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceNoHistoryTest,
                       ModelExecutesButDoesntWriteToHistory) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
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

  EXPECT_FALSE(GetContentAnnotationsForURL(url).has_value());
}

class PageContentAnnotationsServiceModelNotLoadedOnStartupTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceModelNotLoadedOnStartupTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOptimizationHints,
                              features::kPageContentAnnotations},
        /*disabled_features=*/{});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceModelNotLoadedOnStartupTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Win 7 Tests x64: crbug.com/1223172
#if defined(OS_WIN)
#define MAYBE_ModelNotAvailableForFirstNavigation \
  DISABLED_ModelNotAvailableForFirstNavigation
#else
#define MAYBE_ModelNotAvailableForFirstNavigation \
  ModelNotAvailableForFirstNavigation
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceModelNotLoadedOnStartupTest,
                       MAYBE_ModelNotAvailableForFirstNavigation) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", false,
      1);

  LoadAndWaitForModel();

  GURL url2(
      embedded_test_server()->GetURL("a.com", "/hello.html?totally=different"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", 2);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", false,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", true,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService.ModelAvailable", 2);
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace optimization_guide
