// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabIdGroup;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionUnitTestHelper.TabListHolder;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabListEditorGroupAction}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListEditorGroupActionUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private TabGroupModelFilter mGroupFilter;
    @Mock private ActionDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    private MockTabModel mTabModel;
    private TabListEditorAction mAction;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        mAction =
                TabListEditorGroupAction.createAction(
                        RuntimeEnvironment.application,
                        mTabGroupCreationDialogManager,
                        ShowMode.MENU_ONLY,
                        ButtonType.TEXT,
                        IconPosition.START);
        mTabModel = spy(new MockTabModel(mProfile, null));
        when(mGroupFilter.getTabModel()).thenReturn(mTabModel);
        mAction.configure(
                () -> mGroupFilter,
                mSelectionDelegate,
                mDelegate,
                /* editorSupportsActionOnRelatedTabs= */ true);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        assertEquals(
                R.id.tab_list_editor_group_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        assertEquals(
                R.plurals.tab_selection_editor_group_tabs,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        assertEquals(
                R.plurals.accessibility_tab_selection_editor_group_tabs,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        assertNotNull(mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testGroupActionDisabled_NoTabs() {
        List<Integer> tabIds = new ArrayList<>();
        mAction.onSelectionStateChange(tabIds);
        assertEquals(false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testGroupActionDisabled_OneTabGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testGroupActionDisabled_MultipleTabGroupsWithCollaborations() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID
    })
    public void testSingleTabToGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        Tab tab = mTabModel.getTabAt(0);
        assertTrue(mAction.perform());
        verify(mGroupFilter).createSingleTabGroup(tab, true);
        verify(mTabGroupCreationDialogManager).showDialog(tab.getRootId(), mGroupFilter);

        when(mGroupFilter.isTabInTabGroup(tab)).thenReturn(true);
        assertTrue(mAction.perform());
        verify(mGroupFilter, atLeastOnce()).getTabModel();
        verify(mGroupFilter, atLeastOnce()).isTabInTabGroup(any());
        // Ensure this isn't called again.
        verify(mGroupFilter).createSingleTabGroup(tab, true);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID
    })
    public void testGroupActionWithTabs_WillMergingCreateNewGroup() throws Exception {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {3},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {7},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        when(mGroupFilter.willMergingCreateNewGroup(any())).thenReturn(true);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        List<Tab> selectedTabs = holder.getSelectedTabs();
        Tab destinationTab = selectedTabs.get(2);
        assertTrue(mAction.perform());
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(selectedTabs.subList(0, 2), destinationTab, true);
        verify(mTabGroupCreationDialogManager).showDialog(destinationTab.getRootId(), mGroupFilter);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabs_MergedIndividualTabsToNewGroup() throws Exception {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {3},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {7},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        List<Tab> selectedTabs = holder.getSelectedTabs();
        Tab destinationTab = selectedTabs.get(2);
        assertTrue(mAction.perform());
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(selectedTabs.subList(0, 2), destinationTab, true);
        verify(mDelegate).hideByAction();

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        assertTrue(mAction.perform());
        verify(mGroupFilter, times(2))
                .mergeListOfTabsToGroup(selectedTabs.subList(0, 2), destinationTab, true);
        verify(mDelegate, times(2)).hideByAction();
        assertEquals(1, helper.getCallCount());
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeIndividalTabsToExistingGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {3},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {10, 11, 12},
                        /* isGroup= */ true,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        assertEquals(3, holder.getSelectedTabs().size());
        assertEquals(5, holder.getSelectedTabs().get(0).getId());
        assertEquals(3, holder.getSelectedTabs().get(1).getId());
        assertEquals(8, holder.getSelectedTabs().get(2).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(4, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertEquals(4, holder.getSelectedAndRelatedTabs().size());
        assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(2));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(2), true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeGroupToExistingGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {0},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {10, 11, 12},
                        /* isGroup= */ true,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        assertEquals(2, holder.getSelectedTabs().size());
        assertEquals(5, holder.getSelectedTabs().get(0).getId());
        assertEquals(8, holder.getSelectedTabs().get(1).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(5, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertEquals(5, holder.getSelectedAndRelatedTabs().size());
        assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        assertEquals(6, holder.getSelectedAndRelatedTabs().get(4).getId());
        assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(0));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(0), true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeTabsAndGroupsToExistingGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {0},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {10, 11, 12},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        assertEquals(4, holder.getSelectedTabs().size());
        assertEquals(5, holder.getSelectedTabs().get(0).getId());
        assertEquals(8, holder.getSelectedTabs().get(1).getId());
        assertEquals(10, holder.getSelectedTabs().get(2).getId());
        assertEquals(1, holder.getSelectedTabs().get(3).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(9, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertEquals(9, holder.getSelectedAndRelatedTabs().size());
        assertEquals(5, holder.getSelectedAndRelatedTabs().get(0).getId());
        assertEquals(3, holder.getSelectedAndRelatedTabs().get(1).getId());
        assertEquals(8, holder.getSelectedAndRelatedTabs().get(2).getId());
        assertEquals(7, holder.getSelectedAndRelatedTabs().get(3).getId());
        assertEquals(6, holder.getSelectedAndRelatedTabs().get(4).getId());
        assertEquals(10, holder.getSelectedAndRelatedTabs().get(5).getId());
        assertEquals(11, holder.getSelectedAndRelatedTabs().get(6).getId());
        assertEquals(12, holder.getSelectedAndRelatedTabs().get(7).getId());
        assertEquals(1, holder.getSelectedAndRelatedTabs().get(8).getId());
        assertTrue(mAction.perform());
        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(0));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(0), true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeTabsAndGroupsToCollaborationGroup() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {0},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {4},
                        /* isGroup= */ false,
                        /* selected= */ false,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {10, 11, 12},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {1},
                        /* isGroup= */ false,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        List<Tab> selectedTabs = holder.getSelectedTabs();
        assertEquals(4, selectedTabs.size());
        assertEquals(5, selectedTabs.get(0).getId());
        assertEquals(8, selectedTabs.get(1).getId());
        assertEquals(10, selectedTabs.get(2).getId());
        assertEquals(1, selectedTabs.get(3).getId());
        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(9, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        List<Tab> selectedAndRelatedTabs = holder.getSelectedAndRelatedTabs();
        assertEquals(9, selectedAndRelatedTabs.size());
        assertEquals(5, selectedAndRelatedTabs.get(0).getId());
        assertEquals(3, selectedAndRelatedTabs.get(1).getId());
        assertEquals(8, selectedAndRelatedTabs.get(2).getId());
        assertEquals(7, selectedAndRelatedTabs.get(3).getId());
        assertEquals(6, selectedAndRelatedTabs.get(4).getId());
        assertEquals(10, selectedAndRelatedTabs.get(5).getId());
        assertEquals(11, selectedAndRelatedTabs.get(6).getId());
        assertEquals(12, selectedAndRelatedTabs.get(7).getId());
        assertEquals(1, selectedAndRelatedTabs.get(8).getId());
        assertTrue(mAction.perform());

        List<Tab> expectedTabs = selectedAndRelatedTabs;
        // Remove selected destination tab and all related tabs from the expected tabs list. Note
        // that we are merging to the collaboration group.
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(1));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(1), true);
        verify(mDelegate).hideByAction();
    }

    @Test
    @SmallTest
    public void testGroupActionWithTabGroups_MergeCollaborationIsFirst() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ false));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(5, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        assertTrue(mAction.perform());

        List<Tab> expectedTabs = holder.getSelectedAndRelatedTabs();
        // Remove selected destination tab and all related tabs from the expected tabs list. Note
        // that we are merging to the collaboration group.
        List<Tab> destinationAndRelatedTabs =
                mGroupFilter.getRelatedTabList(holder.getSelectedTabIds().get(0));
        expectedTabs.removeAll(destinationAndRelatedTabs);
        verify(mGroupFilter)
                .mergeListOfTabsToGroup(expectedTabs, holder.getSelectedTabs().get(0), true);
        verify(mDelegate).hideByAction();
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testGroupActionWithTabGroups_MergeCollaborationsAsserts() {
        List<TabIdGroup> tabIdGroups = new ArrayList<>();
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {5, 3},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        tabIdGroups.add(
                new TabIdGroup(
                        new int[] {8, 7, 6},
                        /* isGroup= */ true,
                        /* selected= */ true,
                        /* isCollaboration= */ true));
        TabListHolder holder =
                TabListEditorActionUnitTestHelper.configureTabs(
                        mTabModel,
                        mGroupFilter,
                        mTabGroupSyncService,
                        mSelectionDelegate,
                        tabIdGroups,
                        false);

        mAction.onSelectionStateChange(holder.getSelectedTabIds());
        assertEquals(false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        assertEquals(5, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        mAction.perform();
    }
}
