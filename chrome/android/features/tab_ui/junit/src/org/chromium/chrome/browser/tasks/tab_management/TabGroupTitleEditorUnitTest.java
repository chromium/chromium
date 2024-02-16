// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link TabGroupTitleEditor}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS)
public class TabGroupTitleEditorUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

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
    private static final Token GROUP_1_ID = new Token(1L, 2L);
    private static final Token GROUP_2_ID = new Token(2L, 3L);

    @Mock TabModel mTabModel;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabModel mIncognitoTabModel;
    @Mock TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;

    private final ObservableSupplierImpl<TabModelFilter> mTabModelFilterSupplier =
            new ObservableSupplierImpl<>();
    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private Map<String, String> mStorage;
    private TabGroupTitleEditor mTabGroupTitleEditor;

    @Before
    public void setUp() {

        MockitoAnnotations.initMocks(this);

        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE);
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        mTab4 = TabUiUnitTestUtils.prepareTab(TAB4_ID, TAB4_TITLE);
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mIncognitoTabModel).when(mIncognitoTabGroupModelFilter).getTabModel();
        mTabModelFilterSupplier.set(mTabGroupModelFilter);
        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());

        mTabGroupTitleEditor =
                new TabGroupTitleEditor(RuntimeEnvironment.application, mTabModelFilterSupplier) {
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
        assertTrue(mTabModelFilterSupplier.hasObservers());
    }

    @After
    public void tearDown() {
        mTabGroupTitleEditor.destroy();
        assertFalse(mTabModelFilterSupplier.hasObservers());
    }

    @Test
    public void testChangeModels() {
        verify(mTabGroupModelFilter).addObserver(any());
        verify(mTabGroupModelFilter).addTabGroupObserver(any());
        mTabModelFilterSupplier.set(mIncognitoTabGroupModelFilter);
        verify(mIncognitoTabGroupModelFilter).addObserver(any());
        verify(mIncognitoTabGroupModelFilter).addTabGroupObserver(any());
        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());
    }

    @Test
    public void tabClosureCommitted_RootTab_NotDeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that the root tab of the group, tab1, is closed.
        List<Tab> groupAfterClosure = new ArrayList<>(Arrays.asList(mTab2, newTab));
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID))
                .thenReturn(groupAfterClosure.size());
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        assertThat(mStorage.size(), equalTo(1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    public void tabClosureCommitted_NotRootTab_NotDeleteStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);

        // Mock that tab1, tab2, new tab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> groupBeforeClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(groupBeforeClosure, TAB1_ID, GROUP_1_ID);

        // Mock that tab2 is closed and tab2 is not the root tab.
        List<Tab> groupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, newTab));
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID))
                .thenReturn(groupAfterClosure.size());
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        assertThat(mStorage.size(), equalTo(1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS)
    public void tabClosureCommitted_DeleteStoredTitle_GroupSize1NotSupported() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // The stored title should be deleted.
        assertThat(mStorage.size(), equalTo(0));
    }

    @Test
    public void tabClosureCommitted_DeleteStoredTitle_GroupSize1Supported() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that tab1 is closed and the group becomes a single tab.
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab2);

        // The stored title should not be deleted.
        assertThat(mStorage.size(), equalTo(1));

        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

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
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);

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
        createTabGroup(group1, TAB1_ID, GROUP_1_ID);
        List<Tab> group2 = new ArrayList<>(Arrays.asList(mTab3, mTab4));
        createTabGroup(group2, TAB3_ID, GROUP_2_ID);

        mTabGroupModelFilterObserverCaptor.getValue().willMergeTabToGroup(mTab1, TAB3_ID);

        // The stored title should be assigned to the new root id. The title of the source group
        // will not be deleted until the merge is committed, after
        // SnackbarController#onDismissNoAction is called for the UndoGroupSnackbarController.
        assertThat(mStorage.size(), equalTo(2));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB1_ID), equalTo(CUSTOMIZED_TITLE1));
        assertThat(mTabGroupTitleEditor.getTabGroupTitle(TAB3_ID), equalTo(CUSTOMIZED_TITLE1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_GROUP_STABLE_IDS)
    public void tabMoveOutOfGroup_DeleteStoredTitle_GroupSize1NotSupported() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that we are going to ungroup tab1, and the group becomes a single tab after ungroup.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB2_ID);

        // The stored title should be deleted.
        assertThat(mStorage.size(), equalTo(0));
    }

    @Test
    public void tabMoveOutOfGroup_DeleteStoredTitle_GroupSize1Supported() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);
        assertThat(mStorage.size(), equalTo(1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

        // Mock that we are going to ungroup tab1, and the group becomes a single tab after ungroup.
        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab1, TAB2_ID);
        when(mTabGroupModelFilter.getGroupLastShownTab(TAB1_ID)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getGroupLastShownTab(TAB2_ID)).thenReturn(mTab2);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ID)).thenReturn(1);
        when(mTab1.getRootId()).thenReturn(TAB1_ID);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTab2.getRootId()).thenReturn(TAB2_ID);

        // The stored title should not be deleted.
        assertThat(mStorage.size(), equalTo(1));

        mTabGroupModelFilterObserverCaptor.getValue().willMoveTabOutOfGroup(mTab2, TAB2_ID);

        // The stored title should be deleted.
        assertThat(mStorage.size(), equalTo(0));
    }

    @Test
    public void tabMoveOutOfGroup_HandOverStoredTitle() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupTitleEditor.storeTabGroupTitle(TAB1_ID, CUSTOMIZED_TITLE1);

        // Mock that tab1, tab2 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabs, TAB1_ID, GROUP_1_ID);

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

        String expectedTitle =
                RuntimeEnvironment.application
                        .getResources()
                        .getQuantityString(
                                R.plurals.bottom_tab_grid_title_placeholder,
                                relatedTabCount,
                                relatedTabCount);
        assertEquals(
                expectedTitle,
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
