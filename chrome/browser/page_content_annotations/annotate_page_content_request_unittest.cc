// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/annotate_page_content_request.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace page_content_annotations {

class TestPageContentExtractionService : public PageContentExtractionService {
 public:
  explicit TestPageContentExtractionService(
      content::BrowserContext* browser_context)
      : PageContentExtractionService(/*os_crypt_async=*/nullptr,
                                     browser_context->GetPath()) {}
  ~TestPageContentExtractionService() override = default;

  void OnPageContentExtracted(
      content::Page& page,
      const optimization_guide::proto::AnnotatedPageContent&
          annotated_page_content,
      const std::vector<uint8_t>& screenshot_data,
      std::optional<int> tab_id) override {
    last_extracted_content_ = ExtractedPageContentResult(
        annotated_page_content, base::Time::Now(), false, screenshot_data);
    extraction_count_++;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int extraction_count() const { return extraction_count_; }
  const std::optional<ExtractedPageContentResult>& last_extracted_content()
      const {
    return last_extracted_content_;
  }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  int extraction_count_ = 0;
  std::optional<ExtractedPageContentResult> last_extracted_content_;

  base::OnceClosure quit_closure_;
};

std::unique_ptr<KeyedService> BuildTestPageContentExtractionService(
    content::BrowserContext* context) {
  return std::make_unique<TestPageContentExtractionService>(context);
}

class AnnotatePageContentRequestTest : public ChromeRenderViewHostTestHarness {
 public:
  AnnotatePageContentRequestTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    request_ = AnnotatedPageContentRequest::Create(web_contents());
    request_->SetFetchPageContextCallbackForTesting(base::BindRepeating(
        [](content::WebContents&, const FetchPageContextOptions&,
           std::unique_ptr<FetchPageProgressListener>,
           FetchPageContextResultCallback callback) {
          auto page_content =
              std::make_unique<optimization_guide::AIPageContentResult>();
          auto result = std::make_unique<FetchPageContextResult>();
          result->annotated_page_content_result =
              PageContentResultWithEndTime(std::move(*page_content));
          std::move(callback).Run(std::move(result));
        }));
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            PageContentExtractionServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestPageContentExtractionService)},
    };
  }

  void TearDown() override {
    request_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetTriggeringMode(const std::string& mode) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kAnnotatedPageContentExtraction,
        {{"triggering_mode", mode}, {"capture_delay", "0s"}});
  }

  std::unique_ptr<content::MockNavigationHandle> CreateHandle(
      bool committed,
      bool is_same_document) {
    std::unique_ptr<content::MockNavigationHandle> handle =
        std::make_unique<content::MockNavigationHandle>(GURL(), main_rfh());
    handle->set_has_committed(committed);
    handle->set_is_same_document(is_same_document);
    return handle;
  }

  void SimulatePageLoad() {
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.com/"), web_contents());
    navigation->Start();
    request_->PrimaryPageChanged();
    navigation->Commit();
    auto handle = CreateHandle(true, false);
    request_->DidFinishNavigation(handle.get());
    request_->DidStopLoading();
    request_->OnFirstContentfulPaintInPrimaryMainFrame();
  }

  TestPageContentExtractionService* GetExtractionService() {
    return static_cast<TestPageContentExtractionService*>(
        PageContentExtractionServiceFactory::GetForProfile(profile()));
  }

  void WaitForExtraction() {
    base::RunLoop run_loop;
    GetExtractionService()->SetQuitClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AnnotatedPageContentRequest> request_;
};

TEST_F(AnnotatePageContentRequestTest, OnLoadTrigger) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);
  EXPECT_TRUE(GetExtractionService()->last_extracted_content().has_value());

  // Hiding should not trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);
}

TEST_F(AnnotatePageContentRequestTest, OnHiddenTrigger) {
  SetTriggeringMode("on_hidden");

  SimulatePageLoad();

  // Should not extract on load.
  EXPECT_EQ(GetExtractionService()->extraction_count(), 0);

  // Hiding should trigger extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);
  EXPECT_TRUE(GetExtractionService()->last_extracted_content().has_value());

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  // No extraction expected.
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);

  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(GetExtractionService()->extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger) {
  SetTriggeringMode("on_load_and_hidden");

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load.
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);
  EXPECT_TRUE(GetExtractionService()->last_extracted_content().has_value());

  // Hiding should trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(GetExtractionService()->extraction_count(), 2);

  // Showing and hiding again should trigger another extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  // No extraction expected.
  EXPECT_EQ(GetExtractionService()->extraction_count(), 2);

  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(GetExtractionService()->extraction_count(), 3);
}

TEST_F(AnnotatePageContentRequestTest, OnLoadAndHiddenTrigger_LoadWhileHidden) {
  SetTriggeringMode("on_load_and_hidden");

  // Start with the tab hidden.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);

  SimulatePageLoad();
  WaitForExtraction();

  // Should extract on load, even if hidden.
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);
  EXPECT_TRUE(GetExtractionService()->last_extracted_content().has_value());

  // Showing should not trigger extraction.
  web_contents()->WasShown();
  request_->OnVisibilityChanged(content::Visibility::VISIBLE);
  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);

  // Hiding again should trigger another extraction.
  web_contents()->WasHidden();
  request_->OnVisibilityChanged(content::Visibility::HIDDEN);
  WaitForExtraction();
  EXPECT_EQ(GetExtractionService()->extraction_count(), 2);
}

TEST_F(AnnotatePageContentRequestTest, ResetOnNewNavigation) {
  SetTriggeringMode("on_load");

  SimulatePageLoad();
  WaitForExtraction();

  EXPECT_EQ(GetExtractionService()->extraction_count(), 1);

  // New navigation.
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com/2"), web_contents());
  navigation->Start();
  request_->PrimaryPageChanged();
  navigation->Commit();
  auto handle = CreateHandle(true, false);
  request_->DidFinishNavigation(handle.get());
  request_->DidStopLoading();
  request_->OnFirstContentfulPaintInPrimaryMainFrame();
  WaitForExtraction();

  EXPECT_EQ(GetExtractionService()->extraction_count(), 2);
}

}  // namespace page_content_annotations
