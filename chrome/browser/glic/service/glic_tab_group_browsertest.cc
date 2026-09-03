// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicTabGroupBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicTabGroupBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicTabGroups, {{"use_full_tab_embedder", "false"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest, BindAndObserveTabGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  // Ensure we have at least 2 tabs in the group and 1 tab outside the group.
  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  tab_list->ActivateTab(tab1->GetHandle());
  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_EQ(instance->GetTabGroup(), group_id.value());

  // Glic should be bound to both tabs in the group.
  EXPECT_EQ(GetInstanceForTab(tab1), instance);
  EXPECT_EQ(GetInstanceForTab(tab2), instance);

  // Glic should not be bound to the tab outside the group.
  EXPECT_EQ(GetInstanceForTab(tab3), nullptr);

  // Glic sharing manager should still have pinned the tabs.
  GlicSharingManagerInternal& sharing_manager =
      instance->GetSharingManagerInternal();
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab1->GetHandle()));
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab2->GetHandle()));

  // Add a third tab and group it.
  tab_list->AddTabsToGroup(group_id.value(), {tab3->GetHandle()});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(coordinator().GetInstanceForTab(tab3), instance);

  // Ungroup it.
  tab_list->Ungroup({tab3->GetHandle()});
  EXPECT_EQ(coordinator().GetInstanceForTab(tab3), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       UnbindTabGroupDoesNotRemoveTabs) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  tab_list->ActivateTab(tab1->GetHandle());
  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);
  options.target = Target(*tab2, instance->id());
  options.tab_sharing =
      TabSharingOptions({tab2->GetHandle()}, GlicPinTrigger::kContextMenu);
  coordinator().Invoke(std::move(options));
  EXPECT_EQ(tab2->GetGroup(), group_id.value());

  // Calling UnbindTabGroup on the instance should clean up the instance
  // bindings but it should NOT remove the tabs from the actual browser tab
  // group.
  instance->UnbindTabGroup();

  EXPECT_EQ(tab1->GetGroup(), group_id.value());
  EXPECT_EQ(tab2->GetGroup(), group_id.value());
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       SwitchConversationAppliesToTabGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  GlicInstanceImpl* first_instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(first_instance);
  ASSERT_OK(WaitForGlicClient(first_instance));
  glic::mojom::ConversationInfoPtr info = glic::mojom::ConversationInfo::New();
  info->conversation_id = "test_conversation_id_1";
  first_instance->RegisterConversation(std::move(info), base::DoNothing());

  ASSERT_EQ(GetInstanceForTab(tab2), first_instance);
  EXPECT_EQ(first_instance->GetTabGroup(), group_id.value());

  glic::mojom::ConversationInfoPtr new_info =
      glic::mojom::ConversationInfo::New();
  new_info->conversation_id = "test_conversation_id_2";

  base::test::TestFuture<std::optional<mojom::SwitchConversationErrorReason>>
      switch_future;
  coordinator().SwitchConversation(
      *first_instance, ShowOptions::ForSidePanel(*tab1), std::move(new_info),
      switch_future.GetCallback());
  EXPECT_FALSE(switch_future.Get().has_value());

  GlicInstanceImpl* second_instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(second_instance);
  EXPECT_NE(first_instance, second_instance);

  EXPECT_EQ(second_instance->GetTabGroup(), group_id.value());
  EXPECT_EQ(GetInstanceForTab(tab2), second_instance);
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       ShowInstanceForTabGroupReactivatesGlicTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  EXPECT_EQ(coordinator().GetInstanceForTab(tab1), instance);

  tab_list->ActivateTab(tab2->GetHandle());
  ASSERT_OK(RunUntilEqual([&]() { return tab_list->GetActiveTab(); }, tab2));
  EXPECT_NE(tab_list->GetActiveTab(), tab1);

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  EXPECT_EQ(coordinator().GetInstanceForTab(tab2), instance);
}

IN_PROC_BROWSER_TEST_F(
    GlicTabGroupBrowserTest,
    InvokeGlicOnOtherTabInGroupSwapsToSidePanelAndPlaceholder) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  EXPECT_EQ(coordinator().GetInstanceForTab(tab1), instance);

  tab_list->ActivateTab(tab2->GetHandle());
  ASSERT_OK(RunUntilEqual([&]() { return tab_list->GetActiveTab(); }, tab2));

  GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);
  options.target = Target(*tab2, instance->id());
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(coordinator().GetInstanceForTab(tab2), instance);
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest, UnbindTabRemovesTabFromGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  coordinator().UnbindTabFromAnyInstance(tab2);

  EXPECT_EQ(tab1->GetGroup(), group_id.value());
  EXPECT_FALSE(tab2->GetGroup().has_value());
}

}  // namespace glic
