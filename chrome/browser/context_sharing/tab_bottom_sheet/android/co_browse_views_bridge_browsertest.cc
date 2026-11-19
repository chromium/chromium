// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
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
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
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
                                   provider, /*enable_pinch_to_zoom=*/false,
                                   kColorSidePanelContentBackground,
                                   ui::kColorSysSurface1);

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
  EXPECT_FALSE(j_views_before.IsSameObject(env, j_views_after));
}

IN_PROC_BROWSER_TEST_F(CoBrowseViewsBridgeBrowserTest,
                       OnTabInsertedDoesNotCreateViewsBeforeSetWebContents) {
  TabListInterface* tab_list1 = GetTabListInterface();
  ASSERT_TRUE(tab_list1);
  tabs::TabInterface* tab1 = tab_list1->GetActiveTab();
  ASSERT_TRUE(tab1);

  // Wait for the native window to be attached to the tab's `WebContents`.
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

  // 4. Create `CoBrowseViewsBridge` on `tab1` without calling
  // `SetWebContents()` or `CreateCoBrowseViews()`.
  CoBrowseViewsBridge views_bridge(*tab1, TabBottomSheetClientType::kUnknown,
                                   CoBrowseContainerType::kBottomSheet,
                                   provider, /*enable_pinch_to_zoom=*/false,
                                   kColorSidePanelContentBackground,
                                   ui::kColorSysSurface1);
  EXPECT_TRUE(views_bridge.GetCoBrowseViews().is_null());

  // 5. Move `tab1` to the second window.
  tab_list1->MoveTabToWindow(tab1->GetHandle(), window2->GetSessionID(), 0);

  // 6. Verify Java views are still null after moving to the new window.
  EXPECT_TRUE(views_bridge.GetCoBrowseViews().is_null());
}

IN_PROC_BROWSER_TEST_F(CoBrowseViewsBridgeBrowserTest,
                       LoadingPlaceholderCloseRunsCallback) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab = tab_list->GetActiveTab();
  ASSERT_TRUE(tab);

  ASSERT_TRUE(base::test::RunUntil([tab]() {
    return tab->GetContents()->GetTopLevelNativeWindow() != nullptr;
  }));

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> provider =
      Java_TestCoBrowseComponentProvider_Constructor(env);

  int close_count = 0;
  CoBrowseViewsBridge views_bridge(
      *tab, TabBottomSheetClientType::kUnknown,
      CoBrowseContainerType::kBottomSheet, provider,
      /*enable_pinch_to_zoom=*/false, kColorSidePanelContentBackground,
      ui::kColorSysSurface1,
      base::BindLambdaForTesting([&close_count]() { ++close_count; }));

  views_bridge.SetIsLoading(true);
  views_bridge.SetWebContents(nullptr, /*request_focus=*/false);
  ASSERT_FALSE(views_bridge.GetCoBrowseViews().is_null());

  // Clicking close on the Java placeholder runs the native closure.
  Java_TestCoBrowseComponentProvider_clickLoadingPlaceholderClose(env);
  EXPECT_EQ(1, close_count);

  views_bridge.SetIsLoading(false);
}

IN_PROC_BROWSER_TEST_F(CoBrowseViewsBridgeBrowserTest,
                       EmptyPlaceholderFollowsActiveTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab1 = tab_list->GetActiveTab();
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(base::test::RunUntil([tab1]() {
    return tab1->GetContents()->GetTopLevelNativeWindow() != nullptr;
  }));

  std::unique_ptr<content::WebContents> wc2 = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* tab2 = tab_list->InsertWebContentsAt(
      /*index=*/1, std::move(wc2), /*should_pin=*/false, std::nullopt);
  ASSERT_TRUE(tab2);
  ASSERT_TRUE(base::test::RunUntil([tab2]() {
    return tab2->GetContents()->GetTopLevelNativeWindow() != nullptr;
  }));
  tab_list->ActivateTab(tab1->GetHandle());
  ASSERT_TRUE(tab1->IsActivated());
  ASSERT_FALSE(tab2->IsActivated());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TestCoBrowseComponentProvider_setUsePlaceholderForTesting(env, true);
  base::ScopedClosureRunner reset_placeholder(base::BindOnce([]() {
    Java_TestCoBrowseComponentProvider_setUsePlaceholderForTesting(
        base::android::AttachCurrentThread(), false);
  }));

  // Views created for an inactive tab with null WebContents do not show the
  // empty placeholder once loading finishes.
  CoBrowseViewsBridge tab2_views_bridge(
      *tab2, TabBottomSheetClientType::kUnknown,
      CoBrowseContainerType::kSidePanel,
      Java_TestCoBrowseComponentProvider_Constructor(env),
      /*enable_pinch_to_zoom=*/false, kColorSidePanelContentBackground,
      ui::kColorSysSurface1);
  tab2_views_bridge.SetIsLoading(true);
  ASSERT_TRUE(tab2_views_bridge.CreateCoBrowseViews(nullptr,
                                                    /*request_focus=*/false));
  tab2_views_bridge.SetIsLoading(false);
  EXPECT_FALSE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // Views created for the active tab with null WebContents immediately show
  // the empty placeholder without an extra SetWebContents() call.
  std::unique_ptr<content::WebContents> panel_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  CoBrowseViewsBridge tab1_views_bridge(
      *tab1, TabBottomSheetClientType::kUnknown,
      CoBrowseContainerType::kSidePanel,
      Java_TestCoBrowseComponentProvider_Constructor(env),
      /*enable_pinch_to_zoom=*/false, kColorSidePanelContentBackground,
      ui::kColorSysSurface1);
  ASSERT_TRUE(tab1_views_bridge.CreateCoBrowseViews(nullptr,
                                                    /*request_focus=*/false));
  EXPECT_TRUE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // Attach WebContents while tab1 is active, then switch to tab2 and detach
  // tab1's WebContents: tab1's views must not show the empty placeholder.
  tab1_views_bridge.SetWebContents(panel_contents.get(),
                                   /*request_focus=*/false);
  EXPECT_FALSE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  tab_list->ActivateTab(tab2->GetHandle());
  ASSERT_FALSE(tab1->IsActivated());
  ASSERT_TRUE(tab2->IsActivated());
  EXPECT_TRUE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  tab1_views_bridge.SetWebContents(nullptr, /*request_focus=*/false);
  EXPECT_FALSE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // Switching back to tab1 shows tab1's empty placeholder and hides tab2's.
  tab_list->ActivateTab(tab1->GetHandle());
  ASSERT_TRUE(tab1->IsActivated());
  ASSERT_FALSE(tab2->IsActivated());
  EXPECT_TRUE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());
  EXPECT_FALSE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // SetIsLoading(false) on a deactivated tab must not show the placeholder.
  tab2_views_bridge.SetIsLoading(true);
  tab2_views_bridge.SetIsLoading(false);
  EXPECT_FALSE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // WebContentsDestroyed() on a deactivated tab must not show the placeholder.
  tab1_views_bridge.SetWebContents(panel_contents.get(),
                                   /*request_focus=*/false);
  EXPECT_FALSE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());
  tab_list->ActivateTab(tab2->GetHandle());
  ASSERT_FALSE(tab1->IsActivated());
  ASSERT_TRUE(tab2->IsActivated());
  EXPECT_TRUE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());
  panel_contents.reset();
  EXPECT_FALSE(tab1_views_bridge.HasJavaWebContentsForTesting());
  EXPECT_FALSE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());

  // SetWebContents(nullptr) arriving inside the tab model's willSet phase
  // (after OnTabWillDeactivate, while IsActivated() still returns true) must
  // not raise the active flag back to true and show the empty placeholder.
  bool will_deactivate_called = false;
  base::CallbackListSubscription will_deactivate_sub =
      tab2->RegisterWillDeactivate(
          base::BindLambdaForTesting([&](tabs::TabInterface* tab) {
            will_deactivate_called = true;
            EXPECT_TRUE(tab->IsActivated());
            tab2_views_bridge.SetWebContents(nullptr, /*request_focus=*/false);
            EXPECT_FALSE(
                tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());
          }));
  tab_list->ActivateTab(tab1->GetHandle());
  EXPECT_TRUE(will_deactivate_called);
  EXPECT_FALSE(tab2_views_bridge.IsEmptyPlaceholderVisibleForTesting());
  EXPECT_TRUE(tab1_views_bridge.IsEmptyPlaceholderVisibleForTesting());
}

}  // namespace context_sharing
