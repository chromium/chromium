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
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
  browser_display_delegate.AdjustPreviewsStateForNavigation(nullptr,
                                                            &previews_state);
  EXPECT_EQ(blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON, previews_state);
}

TEST(TabWebContentsDelegateAndroidTest,
     AdjustPreviewsStateForNavigationBlocksPreviews) {
  TestTabWebContentsDelegateAndroid standalone_display_delegate(
      blink::mojom::DisplayMode::kStandalone);
  blink::PreviewsState previews_state_standalone =
      blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
  standalone_display_delegate.AdjustPreviewsStateForNavigation(
      nullptr, &previews_state_standalone);
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_OFF, previews_state_standalone);

  TestTabWebContentsDelegateAndroid minimal_ui_display_delegate(
      blink::mojom::DisplayMode::kMinimalUi);
  blink::PreviewsState previews_state_minimal =
      blink::PreviewsTypes::DEFER_ALL_SCRIPT_ON;
  minimal_ui_display_delegate.AdjustPreviewsStateForNavigation(
      nullptr, &previews_state_minimal);
  EXPECT_EQ(blink::PreviewsTypes::PREVIEWS_OFF, previews_state_minimal);
}

}  // namespace android
