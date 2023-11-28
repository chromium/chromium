// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabIdGroup;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabListHolder;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabListEditorGroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorGroupActionUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;
    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mAction =
                TabListEditorGroupAction.createAction(
                        RuntimeEnvironment.application,
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mGroupFilter);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
    }

    private void configure(boolean actionOnRelatedTabs) {
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate, actionOnRelatedTabs);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Assert.assertEquals(
                R.id.tab_list_editor_group_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_group_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_group_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testGroupActionDisabled() {
        configure(false);
        List<Integer> tabIds = new ArrayList<>();
        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        tabIds.add(1);
        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabs_MergedIndividualTabsToNewGroup() throws Exception {
        configure(false);
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(5);
        tabIds.add(3);
        tabIds.add(7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        verify(mGroupFilter).mergeListOfTabsToGroup(tabs, tabs.get(2), true, true);
        verify(mDelegate).hideByAction();

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        verify(mGroupFilter, times(2)).mergeListOfTabsToGroup(tabs, tabs.get(2), true, true);
        verify(mDelegate, times(2)).hideByAction();
        Assert.assertEquals(1, helper.getCallCount());
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeIndividalTabsToExistingGroup() {
        final boolean actionOnRelatedTabs = true;
        configure(actionOnRelatedTabs);
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(new TabIdGroup(new int[] {5}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {3}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {4}, false));
        tabIdGroups.add(new TabIdGroup(new int[] {8, 7}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {10, 11, 12}, false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel, mGroupFilter, mSelectionDelegate, tabIdGroups, false);

        Assert.assertEquals(3, holder.getSelectedTabs().size());
        Assert.assertEquals(5, holder.getSelectedTabs().get(0).getId());
        Assert.assertEquals(3, holder.getSelectedTabs().get(1).getId());
        Assert.assertEquals(8, holder.getSelectedTabs().get(2).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                4, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        Assert.assertEquals(4, holder.getSelectedAndRelatedTabs().size());
        Assert.assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        Assert.assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        Assert.assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        Assert.assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        Assert.assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(2));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(2), true, true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeGroupToExistingGroup() {
        final boolean actionOnRelatedTabs = true;
        configure(actionOnRelatedTabs);
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(new TabIdGroup(new int[] {0}, false));
        tabIdGroups.add(new TabIdGroup(new int[] {5, 3}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {4}, false));
        tabIdGroups.add(new TabIdGroup(new int[] {8, 7, 6}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {10, 11, 12}, false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel, mGroupFilter, mSelectionDelegate, tabIdGroups, false);

        Assert.assertEquals(2, holder.getSelectedTabs().size());
        Assert.assertEquals(5, holder.getSelectedTabs().get(0).getId());
        Assert.assertEquals(8, holder.getSelectedTabs().get(1).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                5, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        Assert.assertEquals(5, holder.getSelectedAndRelatedTabs().size());
        Assert.assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        Assert.assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        Assert.assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        Assert.assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        Assert.assertEquals(6, holder.getSelectedAndRelatedTabs().get(4).getId());
        Assert.assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(0));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(0), true, true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeTabsAndGroupsToExistingGroup() {
        final boolean actionOnRelatedTabs = true;
        configure(actionOnRelatedTabs);
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(new TabIdGroup(new int[] {0}, false));
        tabIdGroups.add(new TabIdGroup(new int[] {5, 3}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {4}, false));
        tabIdGroups.add(new TabIdGroup(new int[] {8, 7, 6}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {10, 11, 12}, true));
        tabIdGroups.add(new TabIdGroup(new int[] {1}, true));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel, mGroupFilter, mSelectionDelegate, tabIdGroups, false);

        Assert.assertEquals(4, holder.getSelectedTabs().size());
        Assert.assertEquals(5, holder.getSelectedTabs().get(0).getId());
        Assert.assertEquals(8, holder.getSelectedTabs().get(1).getId());
        Assert.assertEquals(10, holder.getSelectedTabs().get(2).getId());
        Assert.assertEquals(1, holder.getSelectedTabs().get(3).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                9, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        Assert.assertEquals(9, holder.getSelectedAndRelatedTabs().size());
        Assert.assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        Assert.assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        Assert.assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        Assert.assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        Assert.assertEquals(6, holder.getSelectedAndRelatedTabs().get(4).getId());
        Assert.assertEquals(10, holder.getSelectedAndRelatedTabs().get(5).getId());
        Assert.assertEquals(11, holder.getSelectedAndRelatedTabs().get(6).getId());
        Assert.assertEquals(12, holder.getSelectedAndRelatedTabs().get(7).getId());
        Assert.assertEquals(1, holder.getSelectedAndRelatedTabs().get(8).getId());
        Assert.assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(0));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(0), true, true);
        verify(mDelegate).hideByAction();
    }
}
