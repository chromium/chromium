// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests for {@link TabGroupTitleEditor}.
 */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupTitleEditorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String CUSTOMIZED_TITLE1 = "Some cool tabs";
    private static final String CUSTOMIZED_TITLE2 = "Other cool tabs";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;

    @Mock
    TabModel mTabModel;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private TabImpl mTab4;
    private Map<String, String> mStorage;
    private TabGroupTitleEditor mTabGroupTitleEditor;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        mTab4 = TabUiUnitTestUtils.prepareTab(TAB4_ID, TAB4_TITLE);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        mTabGroupTitleEditor = new TabGroupTitleEditor(
                RuntimeEnvironment.application, mTabModelSelector) {
            @Override
            protected void updateTabGroupTitle(Tab tab, String title) {}

            @Override
            protected void storeTabGroupTitle(int tabRootId, String title) {
                mStorage.put(String.valueOf(tabRootId), title);
            }

            @Override
            protected void deleteTabGroupTitle(int tabRootId) {
                mStorage.remove(String.valueOf(tabRootId));
            }

            @Override
            protected String getTabGroupTitle(int tabRootId) {
                return mStorage.get(String.valueOf(tabRootId));
            }
        };
        mStorage = new HashMap<>();
    }

    @Test
    public void tabClosureCommitted_RootTab_NotDeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        TabImpl newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID);

        // Mock that the root tab of the group, tab1, is closed.
        List<Tab> groupAfterClosure = new ArrayList<>(Arrays.asList(mTab2, newTab));
        doReturn(groupAfterClosure).when(mTabGroupModelFilter).getRelatedTabListForRootId(TAB1_ID);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        assertThat(mStorage.size(), equalTo(1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    public void tabClosureCommitted_NotRootTab_NotDeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);

        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        TabImpl newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> groupBeforeClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(groupBeforeClosure, TAB1_ID);

        // Mock that tab2 is closed and tab2 is not the root tab.
        List<Tab> groupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(groupAfterClosure).when(mTabGroupModelFilter).getRelatedTabListForRootId(TAB1_ID);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        assertThat(mStorage.size(), equalTo(1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    public void tabClosureCommitted_DeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        doReturn(new ArrayList<>(Arrays.asList(mTab2)))
                .when(mTabGroupModelFilter)
                .getRelatedTabListForRootId(TAB1_ID);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // The stored title should be deleted.
        assertThat(mStorage.size(), equalTo(0));
    }

    @Test
    public void tabMergeIntoGroup_NotDeleteStoredTitle() {
        // Mock that we have two stored titles with reference to root ID of tab1 and tab3.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        mTabGroupTitleEditor.storeTabGroupTitle(TAB3_ID, CUSTOMIZED_TITLE2);
        assertThat(mStorage.size(), equalTo(2));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID; tab3 and tab4
        // are in the same group and group root id is TAB3_ID.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The title of the source group will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        assertThat(mStorage.size(), equalTo(2));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB3_ID), equalTo(CUSTOMIZED_TITLE2));
    }

    @Test
    public void tabMergeIntoGroup_HandOverStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID; tab3 and tab4
        // are in the same group and group root id is TAB3_ID.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The stored title should be assigned to the new root id. The title of the source group
        // will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        assertThat(mStorage.size(), equalTo(2));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB3_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    public void tabMoveOutOfGroup_DeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        // Mock that we are going to ungroup tab1, and the group becomes a single tab after ungroup.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB2_ID);

        // The stored title should be deleted.
        assertThat(mStorage.size(), equalTo(0));
    }

    @Test
    public void tabMoveOutOfGroup_HandOverStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);

        // Mock that tab1, tab2 and newTab are in the same group and group root id is TAB1_ID.
        TabImpl newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID);

        // Mock that we are going to ungroup tab1, and the group is still a group after ungroup with
        // root id become TAB2_ID.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB2_ID);

        // The stored title should be assigned to the new root id.
        assertThat(mStorage.size(), equalTo(1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(null));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB2_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    public void testDefaultTitle() {
        int relatedTabCount = 5;

        String expectedTitle = RuntimeEnvironment.application.getResources().getQuantityString(
                R.plurals.bottom_tab_grid_title_placeholder, relatedTabCount, relatedTabCount);
        assertEquals(expectedTitle,
                TabGroupTitleEditor.getDefaultTitle(
                        RuntimeEnvironment.application, relatedTabCount));
    }

    @Test
    public void testIsDefaultTitle() {
        int fourTabsCount = 4;
        String fourTabsTitle =
                TabGroupTitleEditor.getDefaultTitle(RuntimeEnvironment.application, fourTabsCount);
        assertTrue(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, fourTabsCount));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle(fourTabsTitle, 3));
        assertFalse(mTabGroupTitleEditor.isDefaultTitle("Foo", fourTabsCount));
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(tab.getRootId()).thenReturn(rootId);
        }
    }
}
