// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_tab_context_extraction_runner.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "chrome/browser/geic/geic_browser_host_impl.h"
#include "chrome/browser/geic/geic_tab_context_test_util.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace geic {
namespace {

using GetContextResult =
    base::expected<mojom::TabContextDataPtr, mojom::GetTabContextError>;

class TabContextExtractionRunnerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        pwc::mojom::features::kPrivilegedWebContents);
    ChromeRenderViewHostTestHarness::SetUp();

    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    tab_strip_model_delegate_.SetBrowserWindowInterface(
        mock_browser_window_interface_.get());
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    TabStripModel* strip = tab_strip_model_.get();
    ON_CALL(*mock_browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(strip));
    ON_CALL(*mock_browser_window_interface_, GetActiveTabInterface())
        .WillByDefault([strip]() -> tabs::TabInterface* {
          return strip ? strip->GetActiveTab() : nullptr;
        });
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(*mock_browser_window_interface_, GetSessionID())
        .WillByDefault(testing::ReturnRef(session_id_));

    AddTab(GURL("https://example.com/initial"));
    tab_ = tab_strip_model_->GetActiveTab();
    ASSERT_TRUE(tab_);

    // Needed to run unit tests that extract inner text/APC.
    // `ChromeRenderViewHostTestHarness` doesn't have a renderer process running
    // so things end up hanging. Adding this helper is needed to keep the fake
    // agents alive during the test.
    tab_context_helper_ = std::make_unique<TabContextTestHelper>(
        tab_strip_model_->GetActiveWebContents());

    host_impl_ = std::make_unique<GeicBrowserHostImpl>(tab_);
  }

  void TearDown() override {
    runner_.reset();
    tab_context_helper_.reset();
    host_impl_.reset();
    tab_ = nullptr;
    tab_strip_model_->CloseAllTabs();
    tab_strip_model_.reset();
    tab_strip_model_delegate_.SetBrowserWindowInterface(nullptr);
    mock_browser_window_interface_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddTab(const GURL& url) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_contents = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents),
                                        /*foreground=*/true);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents,
                                                               url);
  }

  void NavigateAndCommitActiveTab(const GURL& url) {
    content::WebContents* active_contents =
        tab_strip_model_->GetActiveWebContents();
    CHECK(active_contents);
    content::NavigationSimulator::NavigateAndCommitFromBrowser(active_contents,
                                                               url);
  }

  mojom::TabMetadataPtr CreateTestMetadata() {
    auto metadata = mojom::TabMetadata::New();
    metadata->url = GURL("https://example.com/initial");
    metadata->title = u"Test Page";
    metadata->tab_id = 1;
    metadata->window_id = session_id_.id();
    return metadata;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  tabs::TabModel::PreventFeatureInitializationForTesting prevent_tab_features_;
  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  std::unique_ptr<TabContextTestHelper> tab_context_helper_;
  std::unique_ptr<GeicBrowserHostImpl> host_impl_;
  std::unique_ptr<TabContextExtractionRunner> runner_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  SessionID session_id_ = SessionID::FromSerializedValue(1);
};

TEST_F(TabContextExtractionRunnerTest,
       RequestsNoContentReturnsMetadataImmediately) {
  mojom::TabContextOptions options;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
  EXPECT_EQ(result.value()->metadata->title, u"Test Page");
  EXPECT_EQ(result.value()->metadata->tab_id, 1);
  EXPECT_EQ(result.value()->metadata->window_id, session_id_.id());
  EXPECT_FALSE(result.value()->inner_text.has_value());
  EXPECT_FALSE(result.value()->annotated_page_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_mime_type.has_value());
}

TEST_F(TabContextExtractionRunnerTest, RequestsInnerTextOnly) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
  EXPECT_TRUE(result.value()->inner_text.has_value());
  EXPECT_EQ(result.value()->inner_text, "Extracted text");
  EXPECT_FALSE(result.value()->annotated_page_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_mime_type.has_value());
}

TEST_F(TabContextExtractionRunnerTest,
       RequestsInnerTextWithByteLimitTruncates) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  options.inner_text_bytes_limit = 5;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_TRUE(result.value()->inner_text.has_value());
  EXPECT_EQ(result.value()->inner_text, "Extra");
}

TEST_F(TabContextExtractionRunnerTest, RequestsAnnotatedPageContentOnly) {
  mojom::TabContextOptions options;
  options.include_annotated_page_content = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
  EXPECT_FALSE(result.value()->inner_text.has_value());
  EXPECT_TRUE(result.value()->annotated_page_data.has_value());
  EXPECT_TRUE(result.value()
                  ->annotated_page_data
                  ->As<optimization_guide::proto::AnnotatedPageContent>()
                  .has_value());
  EXPECT_FALSE(result.value()->screenshot_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_mime_type.has_value());
}

TEST_F(TabContextExtractionRunnerTest, RequestsScreenshotOnly) {
  mojom::TabContextOptions options;
  options.include_screenshot = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
  EXPECT_FALSE(result.value()->inner_text.has_value());
  EXPECT_FALSE(result.value()->annotated_page_data.has_value());
  // In unit tests with ChromeRenderViewHostTestHarness, CopyFromSurface on the
  // test RenderWidgetHostView completes with an empty result because no GPU
  // compositor surface exists. Verify that requesting a screenshot succeeds
  // gracefully and returns valid metadata. Real pixel capture and JPEG encoding
  // are covered in browser tests (geic_host_browsertest.cc).
  EXPECT_FALSE(result.value()->screenshot_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_mime_type.has_value());
}

TEST_F(TabContextExtractionRunnerTest, RequestsAllData) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  options.include_annotated_page_content = true;
  options.include_screenshot = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result.value());
  EXPECT_EQ(result.value()->metadata->url, GURL("https://example.com/initial"));
  EXPECT_TRUE(result.value()->inner_text.has_value());
  EXPECT_EQ(result.value()->inner_text, "Extracted text");
  EXPECT_TRUE(result.value()->annotated_page_data.has_value());
  EXPECT_TRUE(result.value()
                  ->annotated_page_data
                  ->As<optimization_guide::proto::AnnotatedPageContent>()
                  .has_value());
  // In unit tests with ChromeRenderViewHostTestHarness, CopyFromSurface on the
  // test RenderWidgetHostView completes with an empty result because no GPU
  // compositor surface exists. Real pixel capture and JPEG encoding are covered
  // in browser tests (geic_host_browsertest.cc).
  EXPECT_FALSE(result.value()->screenshot_data.has_value());
  EXPECT_FALSE(result.value()->screenshot_mime_type.has_value());
}

TEST_F(TabContextExtractionRunnerTest, NullWebContentsReturnsError) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), nullptr, CreateTestMetadata(),
      std::move(options), future.GetCallback());
  runner_->Run();

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kNavigationInProgress);
}

TEST_F(TabContextExtractionRunnerTest, NavigationDuringExtractionReturnsError) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  options.include_annotated_page_content = true;
  options.include_screenshot = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  // Simulate navigation while parallel extractions are running.
  NavigateAndCommitActiveTab(GURL("https://example.com/navigated"));

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kNavigationInProgress);
}

TEST_F(TabContextExtractionRunnerTest,
       HostDestroyedDuringExtractionReturnsTabClosed) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  options.include_annotated_page_content = true;
  options.include_screenshot = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  // Destroy the host before extraction completes.
  host_impl_.reset();

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kTabClosed);
}

TEST_F(TabContextExtractionRunnerTest,
       RunnerDestroyedDuringExtractionReturnsTabClosed) {
  mojom::TabContextOptions options;
  options.include_inner_text = true;
  options.include_annotated_page_content = true;
  options.include_screenshot = true;
  base::test::TestFuture<GetContextResult> future;

  runner_ = std::make_unique<TabContextExtractionRunner>(
      host_impl_->GetWeakPtr(), tab_strip_model_->GetActiveWebContents(),
      CreateTestMetadata(), std::move(options), future.GetCallback());
  runner_->Run();

  // Destroy the runner before extraction completes.
  runner_.reset();

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), mojom::GetTabContextError::kTabClosed);
}

TEST(TabContextExtractionRunnerStaticTest, CalculateScreenshotSize) {
  // Empty viewport returns empty size.
  EXPECT_TRUE(TabContextExtractionRunner::CalculateScreenshotSize(
                  gfx::Size(0, 0), /*max_width=*/800, /*max_height=*/800)
                  .IsEmpty());

  // Zero limits (unconstrained) return empty size (native viewport capture).
  EXPECT_TRUE(TabContextExtractionRunner::CalculateScreenshotSize(
                  gfx::Size(1920, 1080), /*max_width=*/0, /*max_height=*/0)
                  .IsEmpty());

  // Viewport smaller than max limits does not upscale.
  EXPECT_TRUE(TabContextExtractionRunner::CalculateScreenshotSize(
                  gfx::Size(400, 300), /*max_width=*/800, /*max_height=*/800)
                  .IsEmpty());

  // Downscaling preserves 16:9 aspect ratio when width is the limiting factor.
  EXPECT_EQ(TabContextExtractionRunner::CalculateScreenshotSize(
                gfx::Size(1920, 1080), /*max_width=*/800, /*max_height=*/800),
            gfx::Size(800, 450));

  // Downscaling preserves aspect ratio when height is the limiting factor.
  EXPECT_EQ(TabContextExtractionRunner::CalculateScreenshotSize(
                gfx::Size(1920, 1080), /*max_width=*/1000, /*max_height=*/450),
            gfx::Size(800, 450));

  // Only width limit specified preserves aspect ratio.
  EXPECT_EQ(TabContextExtractionRunner::CalculateScreenshotSize(
                gfx::Size(1920, 1080), /*max_width=*/960, /*max_height=*/0),
            gfx::Size(960, 540));

  // Only height limit specified preserves aspect ratio.
  EXPECT_EQ(TabContextExtractionRunner::CalculateScreenshotSize(
                gfx::Size(1920, 1080), /*max_width=*/0, /*max_height=*/540),
            gfx::Size(960, 540));

  // Clamps to at least 1x1 on extreme downscaling.
  EXPECT_EQ(TabContextExtractionRunner::CalculateScreenshotSize(
                gfx::Size(1000, 1000), /*max_width=*/1, /*max_height=*/1),
            gfx::Size(1, 1));
}

}  // namespace
}  // namespace geic
