// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/test/fake_ax_media_app.h"
#include "chrome/browser/accessibility/media_app/test/test_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/services/screen_ai/public/test/fake_screen_ai_annotator.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

namespace {

using ash::media_app_ui::mojom::PageMetadataPtr;
using MojoPageMetadata = ash::media_app_ui::mojom::PageMetadata;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class TestScreenAIInstallState : public screen_ai::ScreenAIInstallState {
 public:
  TestScreenAIInstallState() = default;
  TestScreenAIInstallState(const TestScreenAIInstallState&) = delete;
  TestScreenAIInstallState& operator=(const TestScreenAIInstallState&) = delete;
  ~TestScreenAIInstallState() override = default;

  void SetLastUsageTime() override {}
  void DownloadComponentInternal() override {}
};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

class AXMediaAppUntrustedHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AXMediaAppUntrustedHandlerTest()
      : feature_list_(ash::features::kMediaAppPdfA11yOcr) {}
  AXMediaAppUntrustedHandlerTest(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  AXMediaAppUntrustedHandlerTest& operator=(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  ~AXMediaAppUntrustedHandlerTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, screen_ai::ScreenAIInstallState::GetInstance());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, content::BrowserAccessibilityState::GetInstance());

    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> pageRemote;
    // TODO(b/309860428): Delete MediaApp interface - after we implement all
    // Mojo APIs, it should not be needed any more.
    handler_ = std::make_unique<TestAXMediaAppUntrustedHandler>(
        *web_contents()->GetBrowserContext(), std::move(pageRemote));

    handler_->SetMediaAppForTesting(&fake_media_app_);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    handler_->SetScreenAIAnnotatorForTesting(
        fake_annotator_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, handler_.get());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  TestScreenAIInstallState install_state_;
  screen_ai::test::FakeScreenAIAnnotator fake_annotator_{
      /*create_empty_result=*/true};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  FakeAXMediaApp fake_media_app_;
  std::unique_ptr<TestAXMediaAppUntrustedHandler> handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

TEST_F(AXMediaAppUntrustedHandlerTest, IsAccessibilityEnabled) {
  EXPECT_FALSE(handler_->IsAccessibilityEnabled());
  EXPECT_FALSE(fake_media_app_.IsAccessibilityEnabled());

  accessibility_state_utils::OverrideIsScreenReaderEnabledForTesting(true);
  content::ScopedAccessibilityModeOverride scoped_mode(ui::kAXModeComplete);
  EXPECT_TRUE(handler_->IsAccessibilityEnabled());
  EXPECT_TRUE(fake_media_app_.IsAccessibilityEnabled());
}

TEST_F(AXMediaAppUntrustedHandlerTest, PageMetadataDocumentFirstLoad) {
  const std::vector<std::string> kPageIds{"five", "page", "ids", "in", "list"};
  const size_t kTestNumPages = kPageIds.size();
  std::vector<PageMetadataPtr> fakeMetadata;
  for (size_t i = 0; i < kTestNumPages; ++i) {
    auto page = MojoPageMetadata::New();
    page->id = std::move(kPageIds[i]);
    auto rect = gfx::RectF(0, 0, 10, 15);
    page->rect = std::move(rect);
    fakeMetadata.push_back(std::move(page));
  }

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  auto actual_page_metadata = handler_->GetPageMetadataForTesting();
  EXPECT_EQ(actual_page_metadata.size(), kTestNumPages);

  // Test the added page_num per page matches up to its given ID.
  for (size_t i = 0; i < kTestNumPages; ++i) {
    EXPECT_EQ(actual_page_metadata[kPageIds[i]].id, kPageIds[i]);
  }
}

TEST_F(AXMediaAppUntrustedHandlerTest, PageMetadataNoDuplicatePageIds) {
  // Page IDs should be unique.
  const std::string kDuplicateId = "duplicate";
  std::vector<PageMetadataPtr> fakeMetadata;
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  auto page1 = MojoPageMetadata::New();
  page1->id = std::move(kDuplicateId);
  auto rect = gfx::RectF(0, 0, 10, 15);
  page1->rect = std::move(rect);
  fakeMetadata.push_back(std::move(page1));

  auto page2 = MojoPageMetadata::New();
  page2->id = std::move(kDuplicateId);
  page2->rect = std::move(rect);
  fakeMetadata.push_back(std::move(page2));

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  // Run loop to detect a bad message, if triggered.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bad_message_observer.got_bad_message());
}

TEST_F(AXMediaAppUntrustedHandlerTest, PageMetadataWithDeleteAndUndoDelete) {
  const std::vector<std::string> kPageIds{"pageX", "pageY", "pageZ"};
  const size_t kTestNumPages = kPageIds.size();
  std::vector<PageMetadataPtr> fakeMetadata;
  for (size_t i = 0; i < kTestNumPages; ++i) {
    auto page = MojoPageMetadata::New();
    page->id = std::move(kPageIds[i]);
    auto rect = gfx::RectF(0, 0, 10, 15);
    page->rect = std::move(rect);
    fakeMetadata.push_back(std::move(page));
  }

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  auto actual_page_metadata1 = handler_->GetPageMetadataForTesting();
  EXPECT_EQ(actual_page_metadata1.size(), kTestNumPages);
  // Check the page numbers of each page were set correctly.
  for (size_t i = 1; i <= kTestNumPages; ++i) {
    EXPECT_EQ(actual_page_metadata1[kPageIds[i - 1]].page_num, i);
  }

  // Delete "pageY" by excluding it from the metadata.
  std::vector<PageMetadataPtr> fakeMetadataWithDeletedPage;
  for (size_t i = 0; i < kTestNumPages; ++i) {
    if (kPageIds[i] == "pageY") {
      continue;
    }
    auto page = MojoPageMetadata::New();
    page->id = std::move(kPageIds[i]);
    auto rect = gfx::RectF(0, 0, 10, 15);
    page->rect = std::move(rect);
    fakeMetadataWithDeletedPage.push_back(std::move(page));
  }
  handler_->PageMetadataUpdated(std::move(fakeMetadataWithDeletedPage));

  auto actual_page_metadata2 = handler_->GetPageMetadataForTesting();
  EXPECT_EQ(actual_page_metadata2.size(), kTestNumPages);
  // Check the page numbers of each page were set correctly.
  EXPECT_EQ(actual_page_metadata2["pageX"].page_num, 1u);
  EXPECT_EQ(actual_page_metadata2["pageY"].page_num, 0u);
  EXPECT_EQ(actual_page_metadata2["pageZ"].page_num, 2u);
}

TEST_F(AXMediaAppUntrustedHandlerTest, PageMetadataWithNewPages) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  const std::vector<std::string> kPageIds{"pageX", "pageY"};
  const size_t kTestNumPages = kPageIds.size();
  std::vector<PageMetadataPtr> fakeMetadata;
  for (size_t i = 0; i < kTestNumPages; ++i) {
    auto page = MojoPageMetadata::New();
    page->id = std::move(kPageIds[i]);
    auto rect = gfx::RectF(0, 0, 10, 15);
    page->rect = std::move(rect);
    fakeMetadata.push_back(std::move(page));
  }

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  auto actual_page_metadata = handler_->GetPageMetadataForTesting();
  EXPECT_EQ(actual_page_metadata.size(), kTestNumPages);

  // Add a page with a new ID.
  auto page = MojoPageMetadata::New();
  page->id = std::move("pageZ");
  auto rect = gfx::RectF(0, 0, 10, 15);
  page->rect = std::move(rect);
  fakeMetadata.push_back(std::move(page));

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bad_message_observer.got_bad_message());
}

TEST_F(AXMediaAppUntrustedHandlerTest, DirtyPageOcrOrder) {
  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;

  const std::vector<std::string> kPageIds{"pageW", "pageX", "pageY", "pageZ"};
  const size_t kTestNumPages = kPageIds.size();
  std::vector<PageMetadataPtr> fakeMetadata;
  for (size_t i = 0; i < kTestNumPages; ++i) {
    auto page = MojoPageMetadata::New();
    page->id = std::move(kPageIds[i]);
    auto rect = gfx::RectF(0, 0, 10, 15);
    page->rect = std::move(rect);
    fakeMetadata.push_back(std::move(page));
  }
  handler_->SetDelayCallingOcrNextDirtyPage(true);

  handler_->PageMetadataUpdated(std::move(fakeMetadata));

  // All pages should now be marked dirty, and OCRed in the order they were
  // added.
  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageW");
  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageX");
  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageY");
  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageZ");

  // Each time a page becomes dirty, it should be sent to the back of the queue.
  handler_->PushDirtyPageForTesting("pageX");
  handler_->PushDirtyPageForTesting("pageZ");
  handler_->PushDirtyPageForTesting("pageX");

  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageZ");
  EXPECT_EQ(handler_->PopDirtyPageForTesting(), "pageX");
}

}  // namespace ash::test
