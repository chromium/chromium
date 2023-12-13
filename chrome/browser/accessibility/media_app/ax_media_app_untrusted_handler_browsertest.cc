// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <stdint.h>

#include <memory>

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

// Gap or padding between pages.
constexpr uint64_t kTestPageGap = 2;
// Width of a test page.
constexpr uint64_t kTestPageWidth = 3;
// Height of a test page.
constexpr uint64_t kTestPageHeight = 8;

std::vector<gfx::RectF> CreateFakePageBounds(const uint64_t num_pages) {
  uint64_t x = 0, y = 0;
  std::vector<gfx::RectF> fake_page_locations;
  for (uint64_t i = 0; i < num_pages; ++i) {
    gfx::RectF page_location(x, y + kTestPageGap * i + kTestPageHeight * i,
                             kTestPageWidth, kTestPageHeight);
    fake_page_locations.push_back(page_location);
  }
  return fake_page_locations;
}

class AXMediaAppUntrustedHandlerTest : public InProcessBrowserTest {
 public:
  AXMediaAppUntrustedHandlerTest() : feature_list_(features::kBacklightOcr) {}
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
IN_PROC_BROWSER_TEST_F(AXMediaAppUntrustedHandlerTest, DocumentUpdated) {
  handler_->DocumentUpdated(
      /*page_locations=*/CreateFakePageBounds(3),
      /*dirty_pages=*/{0u, 1u, 2u});
  WaitForOcringPages(3u);

  ASSERT_EQ(3u, fake_media_app_.PageIndicesWithBitmap().size());
  for (size_t i = 0; i < fake_media_app_.PageIndicesWithBitmap().size(); ++i) {
    EXPECT_EQ(static_cast<uint64_t>(i),
              fake_media_app_.PageIndicesWithBitmap()[i]);
  }

  const std::vector<std::unique_ptr<ui::AXTreeManager>>& pages =
      handler_->GetPagesForTesting();
  ASSERT_EQ(3u, pages.size());
  for (const std::unique_ptr<ui::AXTreeManager>& page : pages) {
    ASSERT_NE(nullptr, page.get());
    ASSERT_NE(nullptr, page->ax_tree());
  }

  // Remove the tree data, because its tree ID would change every time the test
  // is run, and because it is unimportant for our test purposes.
  ui::AXTreeData tree_data;
  for (const std::unique_ptr<ui::AXTreeManager>& page : pages) {
    page->ax_tree()->UpdateDataForTesting(tree_data);
  }
  EXPECT_EQ("AXTree\nid=-2 staticText name=Testing (0, 0)-(3, 8)\n",
            pages[0]->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-3 staticText name=Testing (0, 10)-(3, 8)\n",
            pages[1]->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-4 staticText name=Testing (0, 20)-(3, 8)\n",
            pages[2]->ax_tree()->ToString());

  // Relocate or resize all pages, OCR the second page again, and add an
  // additional page to the end. This scenario could happen if the second page
  // was rotated.
  handler_->DocumentUpdated(
      /*page_locations=*/
      {
          gfx::RectF(/*x=*/-3, /*y=*/0, /*width=*/kTestPageWidth,
                     /*height=*/kTestPageHeight),
          gfx::RectF(/*x=*/-3, /*y=*/10,
                     /*width=*/kTestPageHeight,
                     /*height=*/kTestPageWidth),
          gfx::RectF(/*x=*/-3, /*y=*/15,
                     /*width=*/kTestPageWidth,
                     /*height=*/kTestPageHeight),
          gfx::RectF(/*x=*/-3, /*y=*/25,
                     /*width=*/kTestPageWidth,
                     /*height=*/kTestPageHeight),
      },
      /*dirty_pages=*/{1u, 3u});
  WaitForOcringPages(2u);

  ASSERT_EQ(5u, fake_media_app_.PageIndicesWithBitmap().size());
  EXPECT_EQ(1u, fake_media_app_.PageIndicesWithBitmap()[3]);
  EXPECT_EQ(3u, fake_media_app_.PageIndicesWithBitmap()[4]);

  const std::vector<std::unique_ptr<ui::AXTreeManager>>& pages2 =
      handler_->GetPagesForTesting();
  ASSERT_EQ(4u, pages2.size());
  for (const std::unique_ptr<ui::AXTreeManager>& page : pages2) {
    ASSERT_NE(nullptr, page.get());
    ASSERT_NE(nullptr, page->ax_tree());
    page->ax_tree()->UpdateDataForTesting(tree_data);
  }

  EXPECT_EQ("AXTree\nid=-2 staticText name=Testing (-3, 0)-(3, 8)\n",
            pages2[0]->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-5 staticText name=Testing (-3, 10)-(8, 3)\n",
            pages2[1]->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-4 staticText name=Testing (-3, 15)-(3, 8)\n",
            pages2[2]->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-6 staticText name=Testing (-3, 25)-(3, 8)\n",
            pages2[3]->ax_tree()->ToString());
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash::test
