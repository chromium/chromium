// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/font_access_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace {

class FontAccessContextBrowserTest : public InProcessBrowserTest {
 public:
  FontAccessContextBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kFontAccess);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)

IN_PROC_BROWSER_TEST_F(FontAccessContextBrowserTest, BasicEnumerationTest) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(),
                     base::FilePath(FILE_PATH_LITERAL("simple_page.html")))));
  content::FontAccessContext* context = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetMainFrame()
                                            ->GetStoragePartition()
                                            ->GetFontAccessContext();

  base::RunLoop run_loop;
  context->FindAllFonts(base::BindLambdaForTesting(
      [&](blink::mojom::FontEnumerationStatus status,
          std::vector<blink::mojom::FontMetadata> fonts) {
        EXPECT_EQ(status, blink::mojom::FontEnumerationStatus::kOk)
            << "Enumeration expected to be successful.";
        EXPECT_GT(fonts.size(), 0u)
            << "Enumeration expected to yield at least 1 font";
        run_loop.Quit();
      }));
  run_loop.Run();
}

#endif

}  // namespace
