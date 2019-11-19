// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_delegate_android.h"

#include "base/android/jni_android.h"
#include "components/previews/core/previews_experiments.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestTabWebContentsDelegateAndroid
    : public android::TabWebContentsDelegateAndroid {
 public:
  explicit TestTabWebContentsDelegateAndroid(
      blink::mojom::DisplayMode display_mode)
      : TabWebContentsDelegateAndroid(base::android::AttachCurrentThread(),
                                      nullptr) {
    display_mode_ = display_mode;
  }

  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override {
    return display_mode_;
  }

 private:
  blink::mojom::DisplayMode display_mode_ = blink::mojom::DisplayMode::kBrowser;
};

}  // namespace

namespace android {

TEST(TabWebContentsDelegateAndroidTest,
     AdjustPreviewsStateForNavigationAllowsPreviews) {
  TestTabWebContentsDelegateAndroid browser_display_delegate(
      blink::mojom::DisplayMode::kBrowser);
  content::PreviewsState noscript_previews_state = content::NOSCRIPT_ON;
  browser_display_delegate.AdjustPreviewsStateForNavigation(
      nullptr, &noscript_previews_state);
  EXPECT_EQ(content::NOSCRIPT_ON, noscript_previews_state);
}

TEST(TabWebContentsDelegateAndroidTest,
     AdjustPreviewsStateForNavigationBlocksPreviews) {
  TestTabWebContentsDelegateAndroid standalone_display_delegate(
      blink::mojom::DisplayMode::kStandalone);
  content::PreviewsState noscript_previews_state = content::NOSCRIPT_ON;
  standalone_display_delegate.AdjustPreviewsStateForNavigation(
      nullptr, &noscript_previews_state);
  EXPECT_EQ(content::PREVIEWS_OFF, noscript_previews_state);

  TestTabWebContentsDelegateAndroid minimal_ui_display_delegate(
      blink::mojom::DisplayMode::kMinimalUi);
  content::PreviewsState litepage_previews_state = content::SERVER_LITE_PAGE_ON;
  minimal_ui_display_delegate.AdjustPreviewsStateForNavigation(
      nullptr, &litepage_previews_state);
  EXPECT_EQ(content::PREVIEWS_OFF, litepage_previews_state);
}

}  // namespace android
