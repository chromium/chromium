// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"

#include "base/bind_helpers.h"
#include "base/strings/string16.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/previews/core/previews_features.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#endif

class PreviewsLitePageInfoBarDelegateUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PreviewsLitePageInfoBarDelegateUnitTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    MockInfoBarService::CreateForWebContents(web_contents());
  }

  PreviewsLitePageInfoBarDelegate* CreateInfoBar() {
    PreviewsLitePageInfoBarDelegate::Create(web_contents());
    EXPECT_EQ(1U, infobar_service()->infobar_count());
    return static_cast<PreviewsLitePageInfoBarDelegate*>(
        infobar_service()->infobar_at(0)->delegate());
  }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }
};

// TODO(crbug/782740): Test temporarily disabled on Windows because it crashes
// on trybots.
#if defined(OS_WIN)
#define DISABLE_ON_WINDOWS(x) DISABLED_##x
#else
#define DISABLE_ON_WINDOWS(x) x
#endif
TEST_F(PreviewsLitePageInfoBarDelegateUnitTest,
       DISABLE_ON_WINDOWS(InfoBarUserDismissal)) {
  base::HistogramTester tester;
  ConfirmInfoBarDelegate* infobar = CreateInfoBar();

  // Simulate dismissing the infobar.
  infobar->InfoBarDismissed();
  infobar_service()->infobar_at(0)->RemoveSelf();
  EXPECT_EQ(0U, infobar_service()->infobar_count());

  tester.ExpectBucketCount("Previews.LitePageNotificationInfoBar",
                           PreviewsLitePageInfoBarDelegate::kInfoBarDismissed,
                           1);
}

TEST_F(PreviewsLitePageInfoBarDelegateUnitTest,
       DISABLE_ON_WINDOWS(LitePagePreviewInfoBarTest)) {
  base::HistogramTester tester;
  ConfirmInfoBarDelegate* infobar = CreateInfoBar();

  tester.ExpectUniqueSample("Previews.LitePageNotificationInfoBar",
                            PreviewsLitePageInfoBarDelegate::kInfoBarShown, 1);

  // Check the strings.
  ASSERT_TRUE(infobar);
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_LITE_PAGE_PREVIEWS_MESSAGE),
            infobar->GetMessageText());
#if defined(OS_ANDROID)
  ASSERT_EQ(l10n_util::GetStringUTF16(IDS_LITE_PAGE_PREVIEWS_SETTINGS_LINK),
            infobar->GetLinkText());
  ASSERT_EQ(IDR_ANDROID_INFOBAR_PREVIEWS, infobar->GetIconId());
#else
  ASSERT_EQ(base::string16(), infobar->GetLinkText());
  ASSERT_EQ(PreviewsLitePageInfoBarDelegate::kNoIconID, infobar->GetIconId());
#endif
}
