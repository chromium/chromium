// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Tests for {@link TabGroupVisualDataManager}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE
})
public class TabGroupVisualDataManagerUnitTest {

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String CUSTOMIZED_TITLE1 = "Some cool tabs";
    private static final String CUSTOMIZED_TITLE2 = "Other cool tabs";
    private static final int COLOR1_ID = TabGroupColorId.BLUE;
    private static final int COLOR2_ID = TabGroupColorId.RED;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final Token GROUP_1_ID = new Token(1L, 2L);
    private static final Token GROUP_2_ID = new Token(2L, 3L);

    @Mock private Context mContext;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;

    @Captor
    private ArgumentCaptor<TabGroupModelFilterObserver> mIncognitoTabGroupModelFilterObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private TabGroupVisualDataManager mTabGroupVisualDataManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        mTab4 = TabUiUnitTestUtils.prepareTab(TAB4_ID, TAB4_TITLE);

        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(mTab3).when(mTabModelSelector).getTabById(TAB3_ID);
        doReturn(mTab4).when(mTabModelSelector).getTabById(TAB4_ID);
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB1_ID, TAB2_ID, TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        doReturn(mIncognitoTabGroupModelFilter)
                .when(mTabModelFilterProvider)
                .getTabModelFilter(true);

        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doNothing()
                .when(mIncognitoTabGroupModelFilter)
                .addTabGroupObserver(mIncognitoTabGroupModelFilterObserverCaptor.capture());

        mTabGroupVisualDataManager = new TabGroupVisualDataManager(mTabModelSelector);

        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        mTabGroupVisualDataManager.destroy();
    }

    @Test
    public void onFinishingMultipleTabClosure_RootTab_NotDeleteStoredTitle() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that the root tab of the group, tab1, is closed, but tab 2 is still around sharing
        // root ID. (Note that this is a stale value and should have been updated elsewhere to have
        // a new root ID).
        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB1_ID, TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(List.of(mTab1), /* canRestore= */ true);

        // Verify that the title and color were not deleted.
        verify(mTabGroupModelFilter, never()).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupColor(TAB1_ID);
    }

    @Test
    public void onFinishingMultipleTabClosure_NotRootTab_NotDeleteStoredTitle() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> groupBeforeClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(groupBeforeClosure, TAB1_ID, GROUP_1_ID);

        // Mock that tab2 is closed and tab2 is not the root tab.
        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB1_ID, TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(List.of(mTab2), /* canRestore= */ true);

        // Verify that the title and color were not deleted.
        verify(mTabGroupModelFilter, never()).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupColor(TAB1_ID);
    }

    @Test
    public void onFinishingMultipleTabClosure_DeleteStoredTitle_CannotRestore() {
        List<Tab> tabs = List.of(mTab1);
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        doReturn(true).when(mTabGroupModelFilter).isTabGroupHiding(GROUP_1_ID);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);
        // Verify the properties are not deleted yet.
        verify(mTabGroupModelFilter, never()).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupCollapsed(TAB1_ID);

        ShadowLooper.runUiThreadTasks();

        // Verify that the properties are now deleted.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB1_ID);
    }

    @Test
    public void onFinishingMultipleTabClosure_DeleteStoredTitle_GroupSize1Supported() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB1_ID, TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(List.of(mTab2), /* canRestore= */ true);

        // Verify that the title and color were not deleted.
        verify(mTabGroupModelFilter, never()).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupColor(TAB1_ID);

        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(List.of(mTab1), /* canRestore= */ true);

        // Verify that the title and color were deleted.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB1_ID);
    }

    @Test
    public void onFinishingMultipleTabClosure_DeleteStoredTitle_Simultaneous() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(0);
        doReturn(LazyOneshotSupplier.fromValue(Set.of(TAB3_ID, TAB4_ID)))
                .when(mTabGroupModelFilter)
                .getLazyAllRootIdsInComprehensiveModel(any());
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(List.of(mTab1, mTab2), /* canRestore= */ true);

        // Verify that the title and color were deleted.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB1_ID);
    }

    @Test
    public void tabMergeIntoGroup_NotDeleteStoredTitle() {
        when(mTabGroupModelFilter.getTabGroupTitle(TAB1_ID)).thenReturn(CUSTOMIZED_TITLE1);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB3_ID)).thenReturn(CUSTOMIZED_TITLE2);
        when(mTabGroupModelFilter.getTabGroupColor(TAB1_ID)).thenReturn(COLOR1_ID);
        when(mTabGroupModelFilter.getTabGroupColor(TAB3_ID)).thenReturn(COLOR2_ID);

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID; tab3 and tab4
        // are in the same group and group root id is TAB3_ID.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The title of the source group will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        verify(mTabGroupModelFilter, never()).setTabGroupTitle(eq(TAB3_ID), anyString());
        verify(mTabGroupModelFilter, never()).setTabGroupColor(eq(TAB3_ID), anyInt());
    }

    @Test
    public void tabMergeIntoGroup_HandOverStoredTitle() {
        when(mTabGroupModelFilter.getTabGroupTitle(TAB1_ID)).thenReturn(CUSTOMIZED_TITLE1);
        when(mTabGroupModelFilter.getTabGroupTitle(TAB3_ID)).thenReturn(null);
        when(mTabGroupModelFilter.getTabGroupColor(TAB1_ID)).thenReturn(COLOR1_ID);
        when(mTabGroupModelFilter.getTabGroupColor(TAB3_ID))
                .thenReturn(TabGroupColorUtils.INVALID_COLOR_ID);

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID; tab3 and tab4
        // are in the same group and group root id is TAB3_ID.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The stored title should be assigned to the new root id. The title of the source group
        // will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB3_ID), eq(CUSTOMIZED_TITLE1));
        verify(mTabGroupModelFilter).setTabGroupColor(eq(TAB3_ID), eq(COLOR1_ID));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE)
    public void tabMergeIntoGroup_CollapsedWithoutFeature() {
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);
        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupCollapsed(TAB3_ID);
    }

    @Test
    public void tabMoveOutOfGroup_DeleteStoredTitle_GroupSize1Supported() {
        when(mTabGroupModelFilter.getTabGroupTitle(TAB1_ID)).thenReturn(CUSTOMIZED_TITLE1);
        when(mTabGroupModelFilter.getTabGroupColor(TAB1_ID)).thenReturn(COLOR1_ID);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB1_ID)).thenReturn(true);

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that we are going to ungroup tab1, and the group becomes a single tab after ungroup.
        when(mTabGroupModelFilter.getGroupLastShownTab(TAB1_ID)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getGroupLastShownTab(TAB2_ID)).thenReturn(mTab2);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(2);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ID)).thenReturn(2);
        when(mTab1.getRootId()).thenReturn(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTab2.getRootId()).thenReturn(TAB2_ID);

        // Mock the situation that the root tab is not the tab being moved out.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB1_ID);

        // Verify that the title and color were not deleted.
        verify(mTabGroupModelFilter, never()).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter, never()).deleteTabGroupCollapsed(TAB1_ID);

        when(mTabGroupModelFilter.getTabGroupTitle(TAB2_ID)).thenReturn(CUSTOMIZED_TITLE1);
        when(mTabGroupModelFilter.getTabGroupColor(TAB2_ID)).thenReturn(COLOR1_ID);

        // Mock that we are going to ungroup the last tab in a size 1 tab group, and it is the root
        // tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ID)).thenReturn(1);

        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab2, TAB1_ID);

        // Verify that the title and color were deleted.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB2_ID);
        verify(mTabGroupModelFilter).deleteTabGroupColor(TAB2_ID);
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB2_ID);
    }

    @Test
    public void testDidChangeGroupRootId() {
        when(mTabGroupModelFilter.getTabGroupTitle(TAB1_ID)).thenReturn(CUSTOMIZED_TITLE1);
        when(mTabGroupModelFilter.getTabGroupColor(TAB1_ID)).thenReturn(COLOR1_ID);
        when(mTabGroupModelFilter.getTabGroupCollapsed(TAB1_ID)).thenReturn(true);

        // Mock that tab1, tab2 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        mTabGroupModelFilterObserverCaptor.getValue().didChangeGroupRootId(TAB1_ID, TAB2_ID);

        // The stored title should be assigned to the new root id.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB1_ID);
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB2_ID), eq(CUSTOMIZED_TITLE1));
        verify(mTabGroupModelFilter).deleteTabGroupColor(TAB1_ID);
        verify(mTabGroupModelFilter).setTabGroupColor(eq(TAB2_ID), eq(COLOR1_ID));
        verify(mTabGroupModelFilter).deleteTabGroupCollapsed(TAB1_ID);
        verify(mTabGroupModelFilter).setTabGroupCollapsed(TAB2_ID, true);
    }

    private void createTabGroup(List<Tab> tabs, int rootId, @Nullable Token groupId) {
        Tab lastTab = tabs.isEmpty() ? null : tabs.get(0);
        when(mTabGroupModelFilter.getGroupLastShownTab(rootId)).thenReturn(lastTab);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(tabs.size());
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(tabs.size() != 1);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(groupId);
        }
    }
}
