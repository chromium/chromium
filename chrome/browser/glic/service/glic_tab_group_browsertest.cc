// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicTabGroupBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicTabGroupBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kGlicTabGroups);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest, BindAndObserveTabGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  // Ensure we have at least 2 tabs.
  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_EQ(instance->GetTabGroup(), group_id.value());

  // Tab strip should now have 4 tabs: 1 default tab, Glic Tab, and the two
  // blank tabs.
  EXPECT_EQ(tab_list->GetTabCount(), 4);

  tabs::TabInterface* glic_tab = instance->GetGlicTab();
  ASSERT_TRUE(glic_tab);
  EXPECT_EQ(glic_tab->GetGroup(), group_id.value());
  EXPECT_EQ(coordinator().GetInstanceForTab(glic_tab), instance);

  GlicSharingManagerInternal& sharing_manager =
      instance->GetSharingManagerInternal();
  EXPECT_FALSE(sharing_manager.IsTabPinned(glic_tab->GetHandle()));
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab1->GetHandle()));
  EXPECT_TRUE(sharing_manager.IsTabPinned(tab2->GetHandle()));

  // Add a third tab and group it.
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab3);
  tab_list->AddTabsToGroup(group_id.value(), {tab3->GetHandle()});

  EXPECT_EQ(coordinator().GetInstanceForTab(tab3), instance);

  // Ungroup it.
  tab_list->Ungroup({tab3->GetHandle()});
  EXPECT_EQ(coordinator().GetInstanceForTab(tab3), nullptr);

  // Close Glic tab to ensure clean teardown.
  tab_list->CloseTab(glic_tab->GetHandle());
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       DefaultToLastActiveInstanceForTabGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  // 1. First invocation creates Instance A.
  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance_a = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance_a);
  ASSERT_OK(WaitForGlicClient(instance_a));
  EXPECT_EQ(instance_a->GetTabGroup(), group_id.value());

  // Find the Glic tab.
  tabs::TabInterface* glic_tab = instance_a->GetGlicTab();
  ASSERT_TRUE(glic_tab);

  // 2. Close the Glic tab.
  tab_list->CloseTab(glic_tab->GetHandle());

  // The Glic tab is gone.
  EXPECT_EQ(instance_a->GetGlicTab(), nullptr);

  // But the instance should stay alive and still be bound to the group.
  EXPECT_EQ(instance_a->GetTabGroup(), group_id.value());
  EXPECT_EQ(coordinator().GetInstanceForTabGroup(group_id.value()), instance_a);

  // 3. Showing Glic for the tab group again should reuse Instance A.
  GlicInstance* instance_reused =
      coordinator().ShowInstanceForTabGroup(group_id.value());
  EXPECT_EQ(instance_reused, instance_a);
  ASSERT_OK(WaitForGlicClient(instance_a));

  glic_tab = instance_a->GetGlicTab();
  ASSERT_TRUE(glic_tab);
  EXPECT_EQ(glic_tab->GetGroup(), group_id.value());

  // Close Glic tab to ensure clean teardown.
  tab_list->CloseTab(glic_tab->GetHandle());
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       EnsureTabInGroupMovesTabToGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  EXPECT_NE(tab2->GetGroup(), group_id.value());

  // Bind the ungrouped tab. It should automatically be added to the tab group
  // associated with the instance.
  GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);
  options.target = Target(*tab2, instance->id());
  options.tab_sharing =
      TabSharingOptions({tab2->GetHandle()}, GlicPinTrigger::kContextMenu);
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(tab2->GetGroup(), group_id.value());

  tabs::TabInterface* glic_tab = instance->GetGlicTab();
  if (glic_tab) {
    tab_list->CloseTab(glic_tab->GetHandle());
  }
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest, UnbindTabRemovesTabFromGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  EXPECT_NE(tab2->GetGroup(), group_id.value());

  // Bind the ungrouped tab. It should automatically be added to the tab group
  // associated with the instance.
  GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);
  options.target = Target(*tab2, instance->id());
  options.tab_sharing =
      TabSharingOptions({tab2->GetHandle()}, GlicPinTrigger::kContextMenu);
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(tab2->GetGroup(), group_id.value());

  // Unbind tab2 from the instance.
  instance->UnbindTab(tab2);

  // Tab 2 should be removed from the tab group.
  EXPECT_NE(tab2->GetGroup(), group_id.value());
  EXPECT_FALSE(tab2->GetGroup().has_value());

  tabs::TabInterface* glic_tab = instance->GetGlicTab();
  if (glic_tab) {
    tab_list->CloseTab(glic_tab->GetHandle());
  }
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       UnbindTabDoesNotRemoveTabFromOtherGroup) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);
  ASSERT_TRUE(tab3);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  std::optional<tab_groups::TabGroupId> other_group_id =
      tab_list->CreateTabGroup({tab2->GetHandle(), tab3->GetHandle()});
  ASSERT_TRUE(other_group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));
  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  ASSERT_OK(WaitForGlicClient(instance));

  // Bind the ungrouped tab. It should automatically be added to the tab group
  // associated with the instance.
  GlicInvokeOptions options(mojom::InvocationSource::kTabContextMenu);
  options.target = Target(*tab2, instance->id());
  options.tab_sharing =
      TabSharingOptions({tab2->GetHandle()}, GlicPinTrigger::kContextMenu);
  coordinator().Invoke(std::move(options));
  EXPECT_EQ(tab2->GetGroup(), group_id.value());

  // Manually move the tab back to the other group
  tab_list->AddTabsToGroup(other_group_id.value(), {tab2->GetHandle()});
  EXPECT_EQ(tab2->GetGroup(), other_group_id.value());

  // Unbind tab2 from the instance.
  instance->UnbindTab(tab2);

  // Tab 2 should NOT be removed from its current group since it's different.
  EXPECT_EQ(tab2->GetGroup(), other_group_id.value());

  tabs::TabInterface* glic_tab = instance->GetGlicTab();
  if (glic_tab) {
    tab_list->CloseTab(glic_tab->GetHandle());
  }
}

IN_PROC_BROWSER_TEST_F(GlicTabGroupBrowserTest,
                       UnbindTabGroupDoesNotRemoveTabs) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

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

  tabs::TabInterface* glic_tab = instance->GetGlicTab();
  if (glic_tab) {
    tab_list->CloseTab(glic_tab->GetHandle());
  }
}

class GlicTabGroupNoFullTabEmbedderBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicTabGroupNoFullTabEmbedderBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicTabGroups, {{"use_full_tab_embedder", "false"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicTabGroupNoFullTabEmbedderBrowserTest,
                       BindAndObserveTabGroupNoFullTab) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);

  // Ensure we have at least 2 tabs in the group and 1 tab outside the group.
  tabs::TabInterface* tab1 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  tabs::TabInterface* tab3 = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab1);
  ASSERT_TRUE(tab2);
  ASSERT_TRUE(tab3);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_list->CreateTabGroup({tab1->GetHandle(), tab2->GetHandle()});
  ASSERT_TRUE(group_id.has_value());

  ASSERT_TRUE(coordinator().ShowInstanceForTabGroup(group_id.value()));

  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  EXPECT_EQ(instance->GetTabGroup(), group_id.value());

  // Since use_full_tab_embedder is false, Glic should NOT create a Glic tab in
  // the group. Tab count should remain 4 (1 default tab + 2 blank tabs in group
  // + 1 blank tab outside).
  EXPECT_EQ(tab_list->GetTabCount(), 4);

  // There should be no full tab embedder
  EXPECT_EQ(instance->GetGlicTab(), nullptr);

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
}

}  // namespace glic
