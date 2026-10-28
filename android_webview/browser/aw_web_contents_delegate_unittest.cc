// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_contents_delegate.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using base::android::AttachCurrentThread;

namespace android_webview {

namespace {

class AwWebContentsDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    JNIEnv* env = AttachCurrentThread();
    delegate_ = std::make_unique<AwWebContentsDelegate>(
        env, base::android::ScopedJavaLocalRef<jobject>());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AwWebContentsDelegate> delegate_;
};

}  // namespace

// `WebView.loadUrl()` always issues a browser-initiated navigation with the
// `ui::PAGE_TRANSITION_FROM_API` qualifier set. The relaxed prerender
// activation match should apply to such navigations.
TEST_F(AwWebContentsDelegateTest,
       ShouldAllowPartialParamMismatchForBrowserInitiatedLoadUrl) {
  content::MockNavigationHandle handle;
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_API));
  EXPECT_TRUE(delegate_->ShouldAllowPartialParamMismatchOfPrerender2(handle));
}

// A browser-initiated navigation that does not originate from
// `WebView.loadUrl()` (e.g., a reload) should not relax prerender activation
// matching.
TEST_F(AwWebContentsDelegateTest,
       ShouldNotAllowPartialParamMismatchForBrowserInitiatedNonLoadUrl) {
  content::MockNavigationHandle handle;
  handle.set_is_renderer_initiated(false);
  handle.set_page_transition(ui::PAGE_TRANSITION_RELOAD);
  EXPECT_FALSE(delegate_->ShouldAllowPartialParamMismatchOfPrerender2(handle));
}

// A renderer-initiated navigation never originates from `WebView.loadUrl()`,
// so the relaxed prerender activation match should not apply regardless of the
// transition qualifier supplied by the renderer.
TEST_F(AwWebContentsDelegateTest,
       ShouldNotAllowPartialParamMismatchForRendererInitiated) {
  content::MockNavigationHandle handle;
  handle.set_is_renderer_initiated(true);
  handle.set_page_transition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_API));
  EXPECT_FALSE(delegate_->ShouldAllowPartialParamMismatchOfPrerender2(handle));
}

}  // namespace android_webview
