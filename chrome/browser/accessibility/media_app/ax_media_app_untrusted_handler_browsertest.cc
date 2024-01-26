// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <stdint.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"
#include "chrome/browser/accessibility/media_app/test/fake_ax_media_app.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include <vector>

#include "components/services/screen_ai/public/test/fake_screen_ai_annotator.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

namespace {

using ash::media_app_ui::mojom::PageMetadataPtr;

// Gap or padding between pages.
constexpr uint64_t kTestPageGap = 2;
// Width of a test page.
constexpr uint64_t kTestPageWidth = 3;
// Height of a test page.
constexpr uint64_t kTestPageHeight = 8;

// Use letters to generate fake IDs for fake page metadata. If more than 26
// pages are needed, more characters can be added.
constexpr std::string_view kTestPageIds = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Create fake page metadata with pages of the same size positioned 10 units
// spaced apart.
std::vector<PageMetadataPtr> CreateFakePageMetadata(const uint64_t num_pages) {
  if (num_pages > kTestPageIds.size()) {
    LOG(ERROR) << "Can't make more than " << kTestPageIds.size() << " pages.";
  }
  uint64_t x = 0, y = 0;
  std::vector<PageMetadataPtr> fake_page_metadata;
  for (uint64_t i = 0; i < num_pages; ++i) {
    PageMetadataPtr page = ash::media_app_ui::mojom::PageMetadata::New();
    page->id = std::format("Page{}", kTestPageIds[i]);
    page->rect = gfx::RectF(x, y + kTestPageGap * i + kTestPageHeight * i,
                            kTestPageWidth, kTestPageHeight);
    fake_page_metadata.push_back(std::move(page));
  }
  return fake_page_metadata;
}

std::vector<PageMetadataPtr> ClonePageMetadataPtrs(
    const std::vector<PageMetadataPtr>& metadata) {
  std::vector<PageMetadataPtr> fake_page_metadata;
  for (const auto& page : metadata) {
    auto cloned_page = mojo::Clone(page);
    fake_page_metadata.push_back(std::move(cloned_page));
  }
  return fake_page_metadata;
}

class AXMediaAppUntrustedHandlerTest : public InProcessBrowserTest {
 public:
  AXMediaAppUntrustedHandlerTest()
      : feature_list_(ash::features::kMediaAppPdfA11yOcr) {}
  AXMediaAppUntrustedHandlerTest(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  AXMediaAppUntrustedHandlerTest& operator=(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  ~AXMediaAppUntrustedHandlerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_NE(nullptr, AXMediaAppHandlerFactory::GetInstance());
    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> pageRemote;
    mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedPage>
        pageReceiver = pageRemote.InitWithNewPipeAndPassReceiver();

    handler_ = std::make_unique<AXMediaAppUntrustedHandler>(
        *browser()->profile(), std::move(pageRemote));
    // TODO(b/309860428): Delete MediaApp interface - after we implement all
    // Mojo APIs, it should not be needed any more.
    handler_->SetMediaAppForTesting(&fake_media_app_);
    ASSERT_NE(nullptr, handler_.get());
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    handler_->SetIsOcrServiceEnabledForTesting();
    handler_->SetScreenAIAnnotatorForTesting(
        fake_annotator_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  }

  void TearDownOnMainThread() override {
    ASSERT_NE(nullptr, handler_.get());
    handler_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void WaitForOcringPages(uint64_t number_of_pages) const {
    for (uint64_t i = 0; i < number_of_pages; ++i) {
      handler_->FlushForTesting();
    }
  }

  FakeAXMediaApp fake_media_app_;
  std::unique_ptr<AXMediaAppUntrustedHandler> handler_;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  screen_ai::test::FakeScreenAIAnnotator fake_annotator_{
      /*create_empty_result=*/false};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
IN_PROC_BROWSER_TEST_F(AXMediaAppUntrustedHandlerTest, PageMetadataUpdated) {
  const size_t kTestNumPages = 3;
  auto fake_metadata = CreateFakePageMetadata(kTestNumPages);
  handler_->PageMetadataUpdated(ClonePageMetadataPtrs(fake_metadata));
  WaitForOcringPages(kTestNumPages);

  ASSERT_EQ(kTestNumPages, fake_media_app_.PageIdsWithBitmap().size());
  // Make sure the OCR service went through all the pages provided in the
  // earlier call to PageMetadataUpdated(), since on first load all pages are
  // dirty.
  EXPECT_EQ("PageA", fake_media_app_.PageIdsWithBitmap()[0]);
  EXPECT_EQ("PageB", fake_media_app_.PageIdsWithBitmap()[1]);
  EXPECT_EQ("PageC", fake_media_app_.PageIdsWithBitmap()[2]);

  const std::map<const std::string, std::unique_ptr<ui::AXTreeManager>>& pages =
      handler_->GetPagesForTesting();
  ASSERT_EQ(3u, pages.size());
  for (auto const& [_, page] : pages) {
    ASSERT_NE(nullptr, page.get());
    ASSERT_NE(nullptr, page->ax_tree());
  }

  // Remove the tree data, because its tree ID would change every time the test
  // is run, and because it is unimportant for our test purposes.
  ui::AXTreeData tree_data;
  for (auto const& [_, page] : pages) {
    page->ax_tree()->UpdateDataForTesting(tree_data);
  }

  EXPECT_EQ("AXTree\nid=-2 staticText name=Testing (0, 0)-(3, 8)\n",
            pages.at(fake_metadata[0]->id)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-3 staticText name=Testing (0, 10)-(3, 8)\n",
            pages.at(fake_metadata[1]->id)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-4 staticText name=Testing (0, 20)-(3, 8)\n",
            pages.at(fake_metadata[2]->id)->ax_tree()->ToString());

  // Relocate all the pages 3 units to the left and resize the second page. This
  // is similar to a scenario that might happen if the second page was rotated.
  fake_metadata[0]->rect =
      gfx::RectF(/*x=*/-3, /*y=*/0,
                 /*width=*/kTestPageWidth, /*height=*/kTestPageHeight);
  fake_metadata[1]->rect =
      gfx::RectF(/*x=*/-3, /*y=*/10,
                 /*width=*/kTestPageHeight, /*height=*/kTestPageWidth);
  fake_metadata[2]->rect =
      gfx::RectF(/*x=*/-3, /*y=*/15,
                 /*width=*/kTestPageWidth, /*height=*/kTestPageHeight);
  handler_->PageMetadataUpdated(ClonePageMetadataPtrs(fake_metadata));

  // Subsequent calls to PageMetadataUpdated() should not cause any page to be
  // marked as dirty.
  ASSERT_EQ(3u, fake_media_app_.PageIdsWithBitmap().size());

  const std::map<const std::string, std::unique_ptr<ui::AXTreeManager>>&
      pages2 = handler_->GetPagesForTesting();
  ASSERT_EQ(3u, pages2.size());
  for (auto const& [_, page] : pages2) {
    ASSERT_NE(nullptr, page.get());
    ASSERT_NE(nullptr, page->ax_tree());
    page->ax_tree()->UpdateDataForTesting(tree_data);
  }

  EXPECT_EQ("AXTree\nid=-2 staticText name=Testing (-3, 0)-(3, 8)\n",
            pages2.at(fake_metadata[0]->id)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-3 staticText name=Testing (-3, 10)-(8, 3)\n",
            pages2.at(fake_metadata[1]->id)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-4 staticText name=Testing (-3, 15)-(3, 8)\n",
            pages2.at(fake_metadata[2]->id)->ax_tree()->ToString());
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash::test
