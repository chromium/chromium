// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

#include <stdint.h>

#include <map>
#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_handler_factory.h"
#include "chrome/browser/accessibility/media_app/test/fake_ax_media_app.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/gfx/geometry/insets.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/test/fake_screen_ai_annotator.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

namespace {

class AXMediaAppHandlerTest : public InProcessBrowserTest {
 public:
  AXMediaAppHandlerTest() : feature_list_(features::kBacklightOcr) {}
  AXMediaAppHandlerTest(const AXMediaAppHandlerTest&) = delete;
  AXMediaAppHandlerTest& operator=(const AXMediaAppHandlerTest&) = delete;
  ~AXMediaAppHandlerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_NE(nullptr, AXMediaAppHandlerFactory::GetInstance());
    handler_ = AXMediaAppHandlerFactory::GetInstance()->CreateAXMediaAppHandler(
        &fake_media_app_);
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
  std::unique_ptr<AXMediaAppHandler> handler_;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  screen_ai::test::FakeScreenAIAnnotator fake_annotator_{
      /*create_empty_result=*/false};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
IN_PROC_BROWSER_TEST_F(AXMediaAppHandlerTest, DocumentUpdated) {
  handler_->DocumentUpdated(
      /*page_locations=*/{gfx::Insets(1u), gfx::Insets(2u), gfx::Insets(3u)},
      /*dirty_pages=*/{0u, 1u, 5u});
  WaitForOcringPages(3u);

  ASSERT_EQ(3u, fake_media_app_.PageIndicesWithBitmap().size());
  EXPECT_EQ(0u, fake_media_app_.PageIndicesWithBitmap()[0]);
  EXPECT_EQ(1u, fake_media_app_.PageIndicesWithBitmap()[1]);
  EXPECT_EQ(5u, fake_media_app_.PageIndicesWithBitmap()[2]);

  const std::map<uint64_t, std::unique_ptr<ui::AXTreeManager>>& pages =
      handler_->GetPagesForTesting();
  ASSERT_EQ(3u, pages.size());
  ASSERT_NE(nullptr, pages.at(0u));
  ASSERT_NE(nullptr, pages.at(1u));
  ASSERT_NE(nullptr, pages.at(5u));
  ASSERT_NE(nullptr, pages.at(0u)->ax_tree());
  ASSERT_NE(nullptr, pages.at(1u)->ax_tree());
  ASSERT_NE(nullptr, pages.at(5u)->ax_tree());

  // Remove the tree data, because its tree ID would change every time the test
  // is run, and because it is unimportant for our test purposes.
  ui::AXTreeData tree_data;
  pages.at(0u)->ax_tree()->UpdateDataForTesting(tree_data);
  pages.at(1u)->ax_tree()->UpdateDataForTesting(tree_data);
  pages.at(5u)->ax_tree()->UpdateDataForTesting(tree_data);
  EXPECT_EQ("AXTree\nid=-2 staticText name=Testing (1, 1)-(2, 2)\n",
            pages.at(0u)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-3 staticText name=Testing (2, 2)-(4, 4)\n",
            pages.at(1u)->ax_tree()->ToString());
  EXPECT_EQ("AXTree\nid=-4 staticText name=Testing (3, 3)-(6, 6)\n",
            pages.at(5u)->ax_tree()->ToString());

  handler_->DocumentUpdated(
      /*page_locations=*/{gfx::Insets(1u)},
      /*dirty_pages=*/{5u});
  WaitForOcringPages(1u);

  ASSERT_EQ(4u, fake_media_app_.PageIndicesWithBitmap().size());
  EXPECT_EQ(5u, fake_media_app_.PageIndicesWithBitmap()[3]);

  const std::map<uint64_t, std::unique_ptr<ui::AXTreeManager>>& pages2 =
      handler_->GetPagesForTesting();
  ASSERT_EQ(3u, pages2.size());
  ASSERT_NE(nullptr, pages2.at(5u));
  ASSERT_NE(nullptr, pages2.at(5u)->ax_tree());

  pages2.at(5u)->ax_tree()->UpdateDataForTesting(tree_data);
  EXPECT_EQ("AXTree\nid=-5 staticText name=Testing (1, 1)-(2, 2)\n",
            pages2.at(5u)->ax_tree()->ToString());
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash::test
