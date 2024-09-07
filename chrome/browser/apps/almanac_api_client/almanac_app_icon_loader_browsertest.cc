// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_app_icon_loader.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace apps {

class AlmanacAppIconLoaderBrowserTest : public InProcessBrowserTest {
 public:
  AlmanacAppIconLoaderBrowserTest() {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());
    loader_ = std::make_unique<AlmanacAppIconLoader>(*browser()->profile());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();

    loader_.reset();
  }

  std::unique_ptr<AlmanacAppIconLoader> loader_;
};

IN_PROC_BROWSER_TEST_F(AlmanacAppIconLoaderBrowserTest, GetAppIconPngSuccess) {
  base::test::TestFuture<apps::IconValuePtr> future;
  loader_->GetAppIcon(embedded_test_server()->GetURL("/web_apps/blue-192.png"),
                      /*icon_mime_type=*/"image/png",
                      /*icon_masking_allowed=*/true, future.GetCallback());

  apps::IconValue* icon = future.Get<0>().get();
  ASSERT_TRUE(icon);
  EXPECT_EQ(icon->uncompressed.bitmap()->getColor(100, 100), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(AlmanacAppIconLoaderBrowserTest, GetAppIconPngFailure) {
  base::test::TestFuture<apps::IconValuePtr> future;
  loader_->GetAppIcon(embedded_test_server()->GetURL("/does/not/exist.png"),
                      /*icon_mime_type=*/"image/png",
                      /*icon_masking_allowed=*/true, future.GetCallback());

  EXPECT_FALSE(future.Get<0>());
}

IN_PROC_BROWSER_TEST_F(AlmanacAppIconLoaderBrowserTest, GetAppIconSvgSuccess) {
  base::test::TestFuture<apps::IconValuePtr> future;
  loader_->GetAppIcon(embedded_test_server()->GetURL("/favicon/icon.svg"),
                      /*icon_mime_type=*/"image/svg+xml",
                      /*icon_masking_allowed=*/true, future.GetCallback());

  apps::IconValue* icon = future.Get<0>().get();
  ASSERT_TRUE(icon);
  EXPECT_EQ(icon->uncompressed.bitmap()->getColor(100, 100), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_F(AlmanacAppIconLoaderBrowserTest, GetAppIconSvgFailure) {
  base::test::TestFuture<apps::IconValuePtr> future;
  loader_->GetAppIcon(embedded_test_server()->GetURL("/does/not/exist.svg"),
                      /*icon_mime_type=*/"image/svg+xml",
                      /*icon_masking_allowed=*/true, future.GetCallback());

  EXPECT_FALSE(future.Get<0>());
}

IN_PROC_BROWSER_TEST_F(AlmanacAppIconLoaderBrowserTest,
                       GetAppIconSvgWithMissingMimeType) {
  base::test::TestFuture<apps::IconValuePtr> future;
  loader_->GetAppIcon(embedded_test_server()->GetURL("/favicon/icon.svg"),
                      /*icon_mime_type=*/"",
                      /*icon_masking_allowed=*/true, future.GetCallback());

  apps::IconValue* icon = future.Get<0>().get();
  ASSERT_TRUE(icon);
  EXPECT_EQ(icon->uncompressed.bitmap()->getColor(100, 100), SK_ColorBLUE);
}

}  // namespace apps
