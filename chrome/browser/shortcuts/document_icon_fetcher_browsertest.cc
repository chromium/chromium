// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/document_icon_fetcher.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/shortcuts/fetch_icons_from_document_task.h"
#include "chrome/browser/shortcuts/image_test_utils.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace shortcuts {
namespace {
constexpr char kPageNoIcons[] = "/shortcuts/no_icons_page.html";
constexpr char kPageWithIcons[] = "/shortcuts/page_icons.html";

class DocumentIconFetcherTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    default_favicon_server_.AddDefaultHandlers(base::FilePath(
        FILE_PATH_LITERAL("chrome/test/data/shortcuts/default_icon_has_two")));
    ASSERT_TRUE(embedded_https_test_server().Start());
    ASSERT_TRUE(default_favicon_server_.Start());
  }

  GURL GetPageWithDefaultFavicon() {
    return default_favicon_server_.GetURL("/index.html");
  }

 private:
  net::EmbeddedTestServer default_favicon_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(DocumentIconFetcherTest, PageNoIcons) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageNoIcons)));

  base::test::TestFuture<FetchIconsFromDocumentResult> future;
  DocumentIconFetcher::FetchIcons(
      *browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().has_value());
  EXPECT_THAT(
      future.Get().value(),
      testing::ElementsAre(gfx::test::EqualsBitmap(GenerateBitmap(128, U'P'))));
}

IN_PROC_BROWSER_TEST_F(DocumentIconFetcherTest, IconMetadata) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));

  base::test::TestFuture<FetchIconsFromDocumentResult> future;

  DocumentIconFetcher::FetchIcons(
      *browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get().has_value());
  std::vector<SkBitmap> images = future.Get().value();

  SkBitmap expected_green;
  ASSERT_OK_AND_ASSIGN(expected_green,
                       LoadImageFromTestFile(base::FilePath(
                           FILE_PATH_LITERAL("shortcuts/green.png"))));
  SkBitmap expected_noise;
  ASSERT_OK_AND_ASSIGN(expected_noise,
                       LoadImageFromTestFile(base::FilePath(
                           FILE_PATH_LITERAL("shortcuts/noise.png"))));
  EXPECT_THAT(images, testing::UnorderedElementsAre(
                          gfx::test::EqualsBitmap(expected_green),
                          gfx::test::EqualsBitmap(expected_noise)));
}

IN_PROC_BROWSER_TEST_F(DocumentIconFetcherTest, DefaultFavicon) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetPageWithDefaultFavicon()));

  base::test::TestFuture<FetchIconsFromDocumentResult> future;
  DocumentIconFetcher::FetchIcons(
      *browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().has_value());
  std::vector<SkBitmap> images = future.Get().value();
  SkBitmap expected_16;
  ASSERT_OK_AND_ASSIGN(
      expected_16, LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL(
                       "shortcuts/default_icon_has_two/16_favicon_part.png"))));
  SkBitmap expected_32;
  ASSERT_OK_AND_ASSIGN(
      expected_32, LoadImageFromTestFile(base::FilePath(FILE_PATH_LITERAL(
                       "shortcuts/default_icon_has_two/32_favicon_part.png"))));
  EXPECT_THAT(images, testing::UnorderedElementsAre(
                          gfx::test::EqualsBitmap(expected_16),
                          gfx::test::EqualsBitmap(expected_32)));
}

IN_PROC_BROWSER_TEST_F(DocumentIconFetcherTest, WebContentsClosed) {
  base::test::TestFuture<FetchIconsFromDocumentResult> future;
  chrome::NewTab(browser());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));
  DocumentIconFetcher::FetchIcons(
      *browser()->tab_strip_model()->GetActiveWebContents(),
      future.GetCallback());
  chrome::CloseTab(browser());
  ASSERT_TRUE(future.Wait());
  EXPECT_THAT(
      future.Get(),
      base::test::ErrorIs(FetchIconsForDocumentError::kDocumentDestroyed));
}

}  // namespace
}  // namespace shortcuts
