// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link TabGroupVisualDataManager}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID
})
public class TabGroupVisualDataManagerUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String CUSTOMIZED_TITLE1 = "Some cool tabs";
    private static final String CUSTOMIZED_TITLE2 = "Other cool tabs";
    private static final int COLOR1_ID = 0;
    private static final int COLOR2_ID = 1;
    private static final int INVALID_COLOR_ID = -1;
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
    @Mock private SharedPreferences mSharedPreferencesTitle;
    @Mock private SharedPreferences.Editor mEditorTitle;
    @Mock private SharedPreferences.Editor mPutStringEditorTitle;
    @Mock private SharedPreferences.Editor mRemoveEditorTitle;
    @Mock private SharedPreferences mSharedPreferencesColor;
    @Mock private SharedPreferences.Editor mEditorColor;
    @Mock private SharedPreferences.Editor mPutIntEditorColor;
    @Mock private SharedPreferences.Editor mRemoveEditorColor;
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

        doReturn(mSharedPreferencesTitle)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditorTitle).when(mSharedPreferencesTitle).edit();
        doReturn(mRemoveEditorTitle).when(mEditorTitle).remove(any(String.class));
        doReturn(mPutStringEditorTitle)
                .when(mEditorTitle)
                .putString(any(String.class), any(String.class));

        doReturn(mSharedPreferencesColor)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditorColor).when(mSharedPreferencesColor).edit();
        doReturn(mRemoveEditorColor).when(mEditorColor).remove(any(String.class));
        doReturn(mPutIntEditorColor)
                .when(mEditorColor)
                .putInt(any(String.class), any(Integer.class));

        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        mTabGroupVisualDataManager.destroy();
    }

    @Test
    public void tabClosureCommitted_RootTab_NotDeleteStoredTitle() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that the root tab of the group, tab1, is closed.
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        // Verify that the title and color were not deleted.
        verify(mEditorTitle, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle, never()).apply();
        verify(mEditorColor, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor, never()).apply();
    }

    @Test
    public void tabClosureCommitted_NotRootTab_NotDeleteStoredTitle() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> groupBeforeClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(groupBeforeClosure, TAB1_ID, GROUP_1_ID);

        // Mock that tab2 is closed and tab2 is not the root tab.
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // Verify that the title and color were not deleted.
        verify(mEditorTitle, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle, never()).apply();
        verify(mEditorColor, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor, never()).apply();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS)
    public void tabClosureCommitted_DeleteStoredTitle_GroupSize1NotSupported() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // Verify that the title and color were deleted.
        verify(mEditorTitle).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle).apply();
        verify(mEditorColor).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor).apply();
    }

    @Test
    public void tabClosureCommitted_DeleteStoredTitle_GroupSize1Supported() {
        // Assume that CUSTOMIZED_TITLE1 and COLOR1_ID are associated with the tab group.
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // Verify that the title and color were not deleted.
        verify(mEditorTitle, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle, never()).apply();
        verify(mEditorColor, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor, never()).apply();

        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        // Verify that the title and color were deleted.
        verify(mEditorTitle).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle).apply();
        verify(mEditorColor).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor).apply();
    }

    @Test
    public void tabMergeIntoGroup_NotDeleteStoredTitle() {
        // Mock that TITLE1, TITLE2 and COLOR1_ID, COLOR2_ID are associated with the groups.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB1_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB3_ID), null))
                .thenReturn(CUSTOMIZED_TITLE2);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB1_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB3_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR2_ID);

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID; tab3 and tab4
        // are in the same group and group root id is TAB3_ID.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The title of the source group will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        verify(mEditorTitle, never()).putString(eq(String.valueOf(TAB3_ID)), eq(CUSTOMIZED_TITLE1));
        verify(mRemoveEditorTitle, never()).apply();
        verify(mEditorColor, never()).putInt(eq(String.valueOf(TAB3_ID)), eq(COLOR1_ID));
        verify(mRemoveEditorColor, never()).apply();
    }

    @Test
    public void tabMergeIntoGroup_HandOverStoredTitle() {
        // Mock that TITLE1 and COLOR1_ID are associated with the group of TAB1_ID.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB1_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB3_ID), null)).thenReturn(null);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB1_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB3_ID), INVALID_COLOR_ID))
                .thenReturn(INVALID_COLOR_ID);

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
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS)
    public void tabMoveOutOfGroup_DeleteStoredTitle_GroupSize1NotSupported() {
        // Mock that TITLE1 and COLOR1_ID are associated with the group of TAB1_ID.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB1_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB1_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that we are going to ungroup tab1, and the group becomes a single tab after ungroup.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB2_ID);

        // Verify that the title and color were deleted.
        verify(mEditorTitle).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle).apply();
        verify(mEditorColor).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor).apply();
    }

    @Test
    public void tabMoveOutOfGroup_DeleteStoredTitle_GroupSize1Supported() {
        // Mock that TITLE1 and COLOR1_ID are associated with the group of TAB1_ID.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB1_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB1_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);

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
        verify(mEditorTitle, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle, never()).apply();
        verify(mEditorColor, never()).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor, never()).apply();

        // Mock that TITLE1 and COLOR1_ID are associated with the group of TAB1_ID.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB2_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB2_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);

        // Mock that we are going to ungroup the last tab in a size 1 tab group, and it is the root
        // tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ID)).thenReturn(1);

        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab2, TAB1_ID);

        // Verify that the title and color were deleted.
        verify(mEditorTitle).remove(eq(String.valueOf(TAB2_ID)));
        verify(mRemoveEditorTitle).apply();
        verify(mEditorColor).remove(eq(String.valueOf(TAB2_ID)));
        verify(mRemoveEditorColor).apply();
    }

    @Test
    public void testDidChangeGroupRootId() {
        // Mock that TITLE1 and COLOR1_ID are associated with the group of TAB1_ID.
        when(mSharedPreferencesTitle.getString(String.valueOf(TAB1_ID), null))
                .thenReturn(CUSTOMIZED_TITLE1);
        when(mSharedPreferencesColor.getInt(String.valueOf(TAB1_ID), INVALID_COLOR_ID))
                .thenReturn(COLOR1_ID);

        // Mock that tab1, tab2 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        mTabGroupModelFilterObserverCaptor.getValue().didChangeGroupRootId(TAB1_ID, TAB2_ID);

        // The stored title should be assigned to the new root id.
        verify(mEditorTitle).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorTitle).apply();
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB2_ID), eq(CUSTOMIZED_TITLE1));
        verify(mEditorColor).remove(eq(String.valueOf(TAB1_ID)));
        verify(mRemoveEditorColor).apply();
        verify(mTabGroupModelFilter).setTabGroupColor(eq(TAB2_ID), eq(COLOR1_ID));
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
