// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/historical_tab_saver.h"

#include "base/android/jni_android.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace historical_tab_saver {

namespace {

class HistoricalTabSaverBrowserTest : public AndroidBrowserTest {
 protected:
  HistoricalTabSaverBrowserTest() = default;
  ~HistoricalTabSaverBrowserTest() override = default;

  void Navigate() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("/android/google.html")));
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    PlatformBrowserTest::SetUpOnMainThread();
  }
};

}  // namespace

// ----- HistoricalTabSaver TESTS BEGIN -----

// Test WebContentsByteBuffer non-empty object creation.
IN_PROC_BROWSER_TEST_F(HistoricalTabSaverBrowserTest,
                       NonEmptyWebContentsByteBuffer) {
  JNIEnv* env = base::android::AttachCurrentThread();

  Navigate();
  base::android::ScopedJavaLocalRef<jobject> result =
      WebContentsState::GetContentsStateAsByteBuffer(env,
                                                     GetActiveWebContents());

  WebContentsStateByteBuffer web_contents_state =
      WebContentsStateByteBuffer(result, 2);

  EXPECT_FALSE(web_contents_state.backing_buffer.empty());

  WebContentsStateByteBuffer* web_contents_state_ptr = &web_contents_state;
  auto native_contents = WebContentsState::RestoreContentsFromByteBuffer(
      web_contents_state_ptr, true, false);

  ASSERT_TRUE(native_contents);
}

// Test DCHECK crash on null data WebContentsByteBuffer object.
IN_PROC_BROWSER_TEST_F(HistoricalTabSaverBrowserTest,
                       NullDataWebContentsByteBufferCrashCheck) {
#if DCHECK_IS_ON()
  JNIEnv* env = base::android::AttachCurrentThread();

  Navigate();
  base::android::ScopedJavaLocalRef<jobject> result =
      WebContentsState::GetContentsStateAsByteBuffer(env,
                                                     GetActiveWebContents());

  WebContentsStateByteBuffer web_contents_state =
      WebContentsStateByteBuffer(result, 2);
  WebContentsStateByteBuffer* web_contents_state_ptr = &web_contents_state;

  ASSERT_DCHECK_DEATH_WITH(WebContentsState::RestoreContentsFromByteBuffer(
                               web_contents_state_ptr, true, false),
                           "Check failed: data != nullptr (0x0 vs. nullptr)");
#endif
}

// Test DCHECK crash on empty size WebContentsByteBuffer object.
IN_PROC_BROWSER_TEST_F(HistoricalTabSaverBrowserTest,
                       EmptySizeWebContentsByteBufferCrashCheck) {
#if DCHECK_IS_ON()
  JNIEnv* env = base::android::AttachCurrentThread();

  Navigate();
  base::android::ScopedJavaLocalRef<jobject> result =
      WebContentsState::GetContentsStateAsByteBuffer(env,
                                                     GetActiveWebContents());

  WebContentsStateByteBuffer web_contents_state =
      WebContentsStateByteBuffer(result, 2);
  WebContentsStateByteBuffer* web_contents_state_ptr = &web_contents_state;

  ASSERT_DCHECK_DEATH_WITH(WebContentsState::RestoreContentsFromByteBuffer(
                               web_contents_state_ptr, true, false),
                           "Check failed: size > 0 (0 vs. 0)");
#endif
}

}  // namespace historical_tab_saver
