// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_metadata_classifier.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace indigo {
namespace {

class FakeDocumentMetadata : public blink::mojom::DocumentMetadata {
 public:
  FakeDocumentMetadata() = default;
  ~FakeDocumentMetadata() override = default;

  void GetEntities(GetEntitiesCallback callback) override {
    std::move(callback).Run(nullptr);
  }

  void ClassifyProductDetails(
      const std::vector<std::string>& allowed_keywords,
      const std::vector<std::string>& blocked_keywords,
      ClassifyProductDetailsCallback callback) override {
    classify_call_count_++;
    if (defer_reply_) {
      deferred_callback_ = std::move(callback);
    } else {
      std::move(callback).Run(result_ ? result_.Clone() : nullptr);
    }
    if (!classify_called_.IsReady()) {
      classify_called_.SetValue();
    }
  }

  void SetResult(blink::mojom::ProductClassificationResultPtr result) {
    result_ = std::move(result);
  }

  void SetDeferReply(bool defer) { defer_reply_ = defer; }

  ClassifyProductDetailsCallback TakeDeferredCallback() {
    return std::move(deferred_callback_);
  }

  [[nodiscard]] bool WaitForClassifyCall() {
    return classify_called_.WaitAndClear();
  }

  void ClearClassifyCalled() { classify_called_.Clear(); }

  int classify_call_count() const { return classify_call_count_; }

 private:
  blink::mojom::ProductClassificationResultPtr result_;
  bool defer_reply_ = false;
  ClassifyProductDetailsCallback deferred_callback_;
  int classify_call_count_ = 0;
  base::test::TestFuture<void> classify_called_;
};

class IndigoMetadataClassifierTest : public ChromeRenderViewHostTestHarness {
 protected:
  IndigoMetadataClassifierTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kIndigo, {}},
         {features::kIndigoMetadataKeywordHeuristic,
          {{features::kIndigoMetadataKeywordHeuristicSameDocumentNavigationDelay
                .name,
            "800ms"},
           {features::kIndigoMetadataKeywordHeuristicPostDclDelay.name,
            "1000ms"},
           {features::kIndigoMetadataKeywordHeuristicMaxWaitTime.name,
            "2500ms"}}}},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* service = IndigoServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(service);
    service->SetConfigForTesting(MakeConfig());

    classifier_ = std::make_unique<IndigoMetadataClassifier>(
        service,
        base::BindRepeating(&IndigoMetadataClassifierTest::OnResultUpdated,
                            base::Unretained(this)));
  }

  IndigoService::ConfigData MakeConfig() {
    IndigoService::ConfigData config;
    config.allowed_origins = {
        url::Origin::Create(GURL("https://allowed.com")),
    };
    config.allowed_keywords = {"dress", "shirt"};
    config.blocked_keywords = {"giftcard"};
    return config;
  }

  void TearDown() override {
    classifier_.reset();
    receiver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void BindFakeMetadata(content::RenderFrameHost* rfh) {
    content::RenderFrameHostTester::For(rfh)->InitializeRenderFrameIfNeeded();
    auto* remote_interfaces = rfh->GetRemoteInterfaces();
    ASSERT_TRUE(remote_interfaces);

    receiver_ =
        std::make_unique<mojo::Receiver<blink::mojom::DocumentMetadata>>(
            &fake_metadata_);
    service_manager::InterfaceProvider::TestApi test_api(remote_interfaces);
    test_api.SetBinderForName(
        blink::mojom::DocumentMetadata::Name_,
        base::BindRepeating(
            [](mojo::Receiver<blink::mojom::DocumentMetadata>* receiver,
               mojo::ScopedMessagePipeHandle pipe) {
              receiver->Bind(
                  mojo::PendingReceiver<blink::mojom::DocumentMetadata>(
                      std::move(pipe)));
            },
            base::Unretained(receiver_.get())));
  }

  void NavigateTo(const GURL& url) {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    navigation->Start();
    navigation->ReadyToCommit();
    BindFakeMetadata(navigation->GetFinalRenderFrameHost());
    navigation->Commit();
    classifier_->OnNavigationCommitted(web_contents(),
                                       /*is_same_document=*/false);
  }

  void NavigateToSameDocument(const GURL& url) {
    auto navigation =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    navigation->CommitSameDocument();
    classifier_->OnNavigationCommitted(web_contents(),
                                       /*is_same_document=*/true);
  }

  void OnResultUpdated() {
    update_count_++;
    if (!result_updated_.IsReady()) {
      result_updated_.SetValue();
    }
  }

  [[nodiscard]] bool WaitForResultUpdate() {
    bool success = result_updated_.WaitAndClear();
    fake_metadata_.ClearClassifyCalled();
    return success;
  }

  base::test::ScopedFeatureList feature_list_;
  FakeDocumentMetadata fake_metadata_;
  std::unique_ptr<mojo::Receiver<blink::mojom::DocumentMetadata>> receiver_;
  std::unique_ptr<IndigoMetadataClassifier> classifier_;
  int update_count_ = 0;
  base::test::TestFuture<void> result_updated_;
};

TEST_F(IndigoMetadataClassifierTest,
       ResolvesTrueImmediatelyOnDOMContentLoadedForSSRPage) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/product"));

  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata_.SetResult(std::move(result));

  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(WaitForResultUpdate());

  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  EXPECT_EQ(fake_metadata_.classify_call_count(), 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", true, 1);

  // Subsequent onload or timers should not trigger additional classifications
  // or histogram emissions.
  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  task_environment()->FastForwardBy(
      features::kIndigoMetadataKeywordHeuristicMaxWaitTime.Get() +
      features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get());
  EXPECT_EQ(fake_metadata_.classify_call_count(), 1);
  histogram_tester.ExpectTotalCount("Indigo.Discovery.MetadataKeywordHeuristic",
                                    1);
}

TEST_F(IndigoMetadataClassifierTest,
       ResolvesFalseImmediatelyOnBlockedProductEntityAtDCL) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/giftcard"));

  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = true;
  fake_metadata_.SetResult(std::move(result));

  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(WaitForResultUpdate());

  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", false, 1);
}

TEST_F(IndigoMetadataClassifierTest,
       RemainsPendingOnEmptyAtOnloadAndResolvesTrueOnHydrationRetry) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/csr-product"));

  // 1. At DOMContentLoaded, CSR app shell has no JSON-LD yet (returns nullptr).
  fake_metadata_.SetResult(nullptr);
  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  EXPECT_TRUE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 0);
  histogram_tester.ExpectTotalCount("Indigo.Discovery.MetadataKeywordHeuristic",
                                    0);

  // 2. window.onload fires quickly on the app shell before API fetch finishes.
  // Should remain pending because post-DCL hydration retry hasn't run yet.
  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  EXPECT_TRUE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 0);
  histogram_tester.ExpectTotalCount("Indigo.Discovery.MetadataKeywordHeuristic",
                                    0);

  // 3. CSR hydration injects Product JSON-LD before the post-DCL retry fires.
  auto hydrated_result = blink::mojom::ProductClassificationResult::New();
  hydrated_result->allowed_keyword_found = true;
  hydrated_result->blocked_keyword_found = false;
  fake_metadata_.SetResult(std::move(hydrated_result));

  task_environment()->FastForwardBy(
      features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get());
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", true, 1);
}

TEST_F(IndigoMetadataClassifierTest,
       ResolvesFalseAfterBothOnloadAndHydrationRetryReturnNull) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/category-page"));

  fake_metadata_.SetResult(nullptr);

  // 1. DCL returns nullptr -> pending.
  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  EXPECT_TRUE(classifier_->is_pending());

  // 2. window.onload returns nullptr -> still pending until post-DCL retry.
  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  EXPECT_TRUE(classifier_->is_pending());

  // 3. Post-DCL retry returns nullptr -> both onload and retry have completed,
  // so lock in false immediately without waiting for max wait time.
  task_environment()->FastForwardBy(
      features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get());
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", false, 1);
}

TEST_F(IndigoMetadataClassifierTest,
       ResolvesBeforeCueingTimeoutWhenOnloadIsDelayedPastMaxWait) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/slow-ads-page"));

  fake_metadata_.SetResult(nullptr);

  // 1. DCL fires at t=0.
  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  EXPECT_TRUE(classifier_->is_pending());

  const base::TimeDelta post_dcl_delay =
      features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get();
  const base::TimeDelta max_wait_time =
      features::kIndigoMetadataKeywordHeuristicMaxWaitTime.Get();
  ASSERT_GT(max_wait_time, post_dcl_delay);

  // 2. Post-DCL retry fires at t=post_dcl_delay, but window.onload hasn't fired
  // yet due to slow third-party subresources -> remains pending.
  task_environment()->FastForwardBy(post_dcl_delay);
  EXPECT_TRUE(classifier_->is_pending());
  EXPECT_EQ(update_count_, 0);

  // 3. Max wait timer fires at t=max_wait_time (before
  // CheckEligibilityForCueing's 3s hard timeout). Locks in false even though
  // window.onload never fired.
  task_environment()->FastForwardBy(max_wait_time - post_dcl_delay);
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", false, 1);
}

TEST_F(IndigoMetadataClassifierTest,
       SameDocumentNavigationChecksAtDelayAndMaxWait) {
  base::HistogramTester histogram_tester;
  NavigateTo(GURL("https://allowed.com/home"));

  // Navigate same-document to a PDP route.
  NavigateToSameDocument(GURL("https://allowed.com/spa-product"));
  EXPECT_TRUE(classifier_->is_pending());

  const base::TimeDelta same_doc_delay =
      features::kIndigoMetadataKeywordHeuristicSameDocumentNavigationDelay
          .Get();
  const base::TimeDelta max_wait_time =
      features::kIndigoMetadataKeywordHeuristicMaxWaitTime.Get();
  ASSERT_GT(max_wait_time, same_doc_delay);

  // At same_doc_delay, SPA route transition hasn't injected new JSON-LD yet.
  fake_metadata_.SetResult(nullptr);
  task_environment()->FastForwardBy(same_doc_delay);
  EXPECT_TRUE(classifier_->is_pending());
  EXPECT_EQ(update_count_, 0);

  // Before max_wait_time, SPA injects product JSON-LD.
  auto spa_result = blink::mojom::ProductClassificationResult::New();
  spa_result->allowed_keyword_found = true;
  spa_result->blocked_keyword_found = false;
  fake_metadata_.SetResult(std::move(spa_result));

  task_environment()->FastForwardBy(max_wait_time - same_doc_delay);
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  histogram_tester.ExpectUniqueSample(
      "Indigo.Discovery.MetadataKeywordHeuristic", true, 1);
}

TEST_F(IndigoMetadataClassifierTest, IneligibleOriginResolvesFalseImmediately) {
  NavigateTo(GURL("https://unallowed-domain.com/product"));

  // Ineligible origin resolves synchronously at OnNavigationCommitted without
  // starting timers or invoking on_result_updated_.
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_FALSE(classifier_->matches());
  EXPECT_EQ(update_count_, 0);
  EXPECT_EQ(fake_metadata_.classify_call_count(), 0);

  classifier_->OnDOMContentLoaded(main_rfh());
  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_EQ(fake_metadata_.classify_call_count(), 0);
}

TEST_F(IndigoMetadataClassifierTest,
       ResolvesTrueWhenConfigLoadsAfterDOMContentLoaded) {
  auto* service = IndigoServiceFactory::GetForProfile(profile());
  service->ResetConfigForTesting();

  NavigateTo(GURL("https://allowed.com/product"));
  EXPECT_TRUE(classifier_->is_pending());

  // At DOMContentLoaded, config is not yet loaded. Classification should remain
  // pending rather than prematurely locking in false.
  classifier_->OnDOMContentLoaded(main_rfh());
  EXPECT_TRUE(classifier_->is_pending());
  EXPECT_EQ(update_count_, 0);
  EXPECT_EQ(fake_metadata_.classify_call_count(), 0);

  // Config finishes loading before window.onload.
  service->SetConfigForTesting(MakeConfig());
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata_.SetResult(std::move(result));

  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_TRUE(WaitForResultUpdate());
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
  EXPECT_EQ(fake_metadata_.classify_call_count(), 1);
}

TEST_F(IndigoMetadataClassifierTest,
       SupersedesInFlightRequestWhenNewerMilestoneFires) {
  NavigateTo(GURL("https://allowed.com/product"));
  fake_metadata_.SetResult(nullptr);
  classifier_->OnDOMContentLoaded(main_rfh());
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());

  // Hold the reply for the post-DCL retry so it remains in flight when onload
  // fires.
  fake_metadata_.SetDeferReply(true);
  task_environment()->FastForwardBy(
      features::kIndigoMetadataKeywordHeuristicPostDclDelay.Get());
  ASSERT_TRUE(fake_metadata_.WaitForClassifyCall());
  auto stale_retry_callback = fake_metadata_.TakeDeferredCallback();

  // Now window.onload fires with a valid Product entity. Because
  // pending_request_ resets the CancelableOnceCallback, the stale retry reply
  // is cancelled and cannot prematurely lock in false.
  fake_metadata_.SetDeferReply(false);
  auto result = blink::mojom::ProductClassificationResult::New();
  result->allowed_keyword_found = true;
  result->blocked_keyword_found = false;
  fake_metadata_.SetResult(std::move(result));

  classifier_->OnDocumentOnLoadCompletedInPrimaryMainFrame();
  ASSERT_TRUE(WaitForResultUpdate());
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);

  // Running the stale callback from the earlier milestone is a no-op.
  std::move(stale_retry_callback).Run(nullptr);
  EXPECT_FALSE(classifier_->is_pending());
  EXPECT_TRUE(classifier_->matches());
  EXPECT_EQ(update_count_, 1);
}

}  // namespace
}  // namespace indigo
