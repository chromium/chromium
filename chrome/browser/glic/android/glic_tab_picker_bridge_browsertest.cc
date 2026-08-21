// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/android/glic_tab_picker_bridge.h"

#include <optional>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicTabPickerBridgeBrowserTest : public GlicBrowserTest {
 public:
  GlicTabPickerBridgeBrowserTest() = default;
  ~GlicTabPickerBridgeBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicTabPickerBridgeBrowserTest,
                       NullWindowRunsCallbackGracefully) {
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab);

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  base::test::TestFuture<void> future;
  GlicTabPickerBridge::OpenTabPicker(
      /*window_android=*/nullptr,
      instance->GetSharingManagerInternal().GetWeakPtr(), future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicTabPickerBridgeBrowserTest,
                       OnTabPickerCompletedUpdatesSharingManager) {
  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab2);

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicSharingManagerInternal& sharing_manager =
      instance->GetSharingManagerInternal();
  sharing_manager.UnpinAllTabs(GlicUnpinTrigger::kUnknown);

  TabAndroid* tab1_android = TabAndroid::FromTabHandle(tab1->GetHandle());
  TabAndroid* tab2_android = TabAndroid::FromTabHandle(tab2->GetHandle());
  ASSERT_TRUE(tab1_android);
  ASSERT_TRUE(tab2_android);

  // Initially pin tab1.
  EXPECT_TRUE(
      sharing_manager.PinTabs({tab1->GetHandle()}, GlicPinTrigger::kTabPicker));
  EXPECT_EQ(sharing_manager.GetPinnedTabs().size(), 1u);
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab1->GetHandle()));
  EXPECT_FALSE(sharing_manager.IsTabPinned(tab2->GetHandle()));

  // Simulate picker completion where user unpinned tab1 and pinned tab2.
  base::test::TestFuture<void> future;
  GlicTabPickerBridge::OnTabPickerCompletedForTesting(
      /*initial_selected=*/{tab1_android}, sharing_manager.GetWeakPtr(),
      future.GetCallback(),
      /*final_selected=*/std::vector<TabAndroid*>{tab2_android});
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(sharing_manager.GetPinnedTabs().size(), 1u);
  EXPECT_FALSE(sharing_manager.IsTabPinned(tab1->GetHandle()));
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab2->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicTabPickerBridgeBrowserTest,
                       OnTabPickerCanceledLeavesPinnedTabsUnchanged) {
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab);

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  GlicSharingManagerInternal& sharing_manager =
      instance->GetSharingManagerInternal();
  sharing_manager.UnpinAllTabs(GlicUnpinTrigger::kUnknown);

  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab->GetHandle());
  ASSERT_TRUE(tab_android);

  EXPECT_TRUE(
      sharing_manager.PinTabs({tab->GetHandle()}, GlicPinTrigger::kTabPicker));
  EXPECT_EQ(sharing_manager.GetPinnedTabs().size(), 1u);

  // Simulate picker cancellation (final_selected is std::nullopt).
  base::test::TestFuture<void> future;
  GlicTabPickerBridge::OnTabPickerCompletedForTesting(
      /*initial_selected=*/{tab_android}, sharing_manager.GetWeakPtr(),
      future.GetCallback(), /*final_selected=*/std::nullopt);
  EXPECT_TRUE(future.Wait());

  // Tab should remain pinned.
  EXPECT_EQ(sharing_manager.GetPinnedTabs().size(), 1u);
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab->GetHandle()));
}

}  // namespace glic
