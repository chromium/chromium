// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_container_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_client_type.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/test_jni_headers/TestCoBrowseComponentProvider_jni.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_sharing {

class CoBrowseViewsBridgeBrowserTest : public PlatformBrowserTest {
 public:
  CoBrowseViewsBridgeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(chrome::android::kTabBottomSheet);
  }
  ~CoBrowseViewsBridgeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "CoBrowseViewsBridgeBrowserTest requires Android S+";
    }
    PlatformBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    PlatformBrowserTest::SetUpDefaultCommandLine(command_line);
    // Disable the first-run experience (FRE) so that when a function under
    // test launches an Intent for ChromeTabbedActivity, ChromeTabbedActivity
    // will be shown instead of FirstRunActivity.
    command_line->AppendSwitch("disable-fre");
  }
};

IN_PROC_BROWSER_TEST_F(
    CoBrowseViewsBridgeBrowserTest,
    OnTabInsertedProactivelyCreatesViewsWhenWebContentsNull) {
  TabListInterface* tab_list1 = GetTabListInterface();
  ASSERT_TRUE(tab_list1);
  tabs::TabInterface* tab1 = tab_list1->GetActiveTab();
  ASSERT_TRUE(tab1);

  // Wait for the native window to be attached to the tab's WebContents.
  ASSERT_TRUE(base::test::RunUntil([tab1]() {
    return tab1->GetContents()->GetTopLevelNativeWindow() != nullptr;
  }));

  // 1. Create 2 tabs in the first window.
  std::unique_ptr<content::WebContents> wc2 = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* tab2 = tab_list1->InsertWebContentsAt(
      /*index=*/1, std::move(wc2), /*should_pin=*/false, std::nullopt);
  ASSERT_TRUE(tab2);

  // 2. Create a second window asynchronously.
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(
      BrowserWindowCreateParams(*GetProfile(), /*from_user_gesture=*/false),
      future.GetCallback());
  BrowserWindowInterface* window2 = future.Get();
  ASSERT_TRUE(window2);
  TabListInterface* tab_list2 = TabListInterface::From(window2);
  ASSERT_TRUE(tab_list2);

  // 3. Create the provider.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> provider =
      Java_TestCoBrowseComponentProvider_Constructor(env);

  // 4. Create CoBrowseViewsBridge on tab1.
  CoBrowseViewsBridge views_bridge(*tab1, TabBottomSheetClientType::kUnknown,
                                   CoBrowseContainerType::kBottomSheet,
                                   provider);

  // 5. Set web contents to nullptr (inactive state, placeholder is shown).
  views_bridge.SetWebContents(nullptr, /*request_focus=*/false);
  base::android::ScopedJavaLocalRef<jobject> j_views_before =
      views_bridge.GetCoBrowseViews();
  ASSERT_FALSE(j_views_before.is_null());

  // 6. Move tab1 to the second window.
  tab_list1->MoveTabToWindow(tab1->GetHandle(), window2->GetSessionID(), 0);

  // 7. Verify the Java views were proactively recreated on the new window
  // (which means j_views_before and j_views_after refer to different Java
  // objects).
  base::android::ScopedJavaLocalRef<jobject> j_views_after =
      views_bridge.GetCoBrowseViews();
  ASSERT_FALSE(j_views_after.is_null());
  EXPECT_FALSE(env->IsSameObject(j_views_before.obj(), j_views_after.obj()));
}

}  // namespace context_sharing
