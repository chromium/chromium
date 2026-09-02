// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Tests for {@link GroupWindowChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupWindowCheckerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mSyncService;
    @Mock private TabModel mTabModel;
    @Spy private TabList mTabList;
    @Mock private Tab mTab1;
    private Context mContext;
    private GroupWindowChecker mSyncUtils;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);

        mSyncUtils = new GroupWindowChecker(mContext, mSyncService, mTabModel);
    }

    @Test
    public void testGetSortedGroupList() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = createSavedTabGroup(token1, "title1");
        SavedTabGroup group2 = createSavedTabGroup(token2, "title2");

        group1.localId = null;
        group2.localId = null;
        group1.savedTabs.add(new SavedTabGroupTab());
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mSyncService.getGroup("id1")).thenReturn(group1);
        when(mSyncService.getGroup("id2")).thenReturn(group2);

        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        this::tabGroupSelectionPredicate,
                        (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(2, sortedList.size());
        assertEquals("title1", sortedList.get(0).title);
        assertEquals("title2", sortedList.get(1).title);
    }

    @Test
    public void testGetSortedGroupListMultipleWindows() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = createSavedTabGroup(token1, "title1");
        SavedTabGroup group2 = createSavedTabGroup(token2, "title2");

        group1.localId = new LocalTabGroupId(token1);
        group2.localId = new LocalTabGroupId(token2);
        group1.savedTabs.add(new SavedTabGroupTab());
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mSyncService.getGroup("id1")).thenReturn(group1);
        when(mSyncService.getGroup("id2")).thenReturn(group2);
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token1);

        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        this::tabGroupSelectionPredicate,
                        (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(1, sortedList.size());
        assertEquals("title1", sortedList.get(0).title);
    }

    @Test
    public void testGetSortedGroupList_empty() {
        when(mSyncService.getAllGroupIds()).thenReturn(new String[] {});
        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        this::tabGroupSelectionPredicate,
                        (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));
        assertEquals(0, sortedList.size());
    }

    @Test
    public void testGetState_hidden() {
        SavedTabGroup group = new SavedTabGroup();
        group.localId = null;

        @GroupWindowState int state = mSyncUtils.getState(group);
        assertEquals(GroupWindowState.HIDDEN, state);
    }

    @Test
    public void testGetState_InCurrent() {
        Token token = Token.createRandom();
        SavedTabGroup group = createSavedTabGroup(token, "title1");
        group.localId = new LocalTabGroupId(token);

        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token);

        @GroupWindowState int state = mSyncUtils.getState(group);
        assertEquals(GroupWindowState.IN_CURRENT, state);
    }

    @Test
    public void testGetState_InCurrentClosing() {
        Token token = Token.createRandom();
        SavedTabGroup group = createSavedTabGroup(token, "title1");
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.isClosing()).thenReturn(true);

        @GroupWindowState int state = mSyncUtils.getState(group);
        assertEquals(GroupWindowState.IN_CURRENT_CLOSING, state);
    }

    @Test
    public void testGetState_InAnother() {
        Token token = Token.createRandom();
        SavedTabGroup group = createSavedTabGroup(token, "title1");

        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(new Token(200L, 1L));

        @GroupWindowState int state = mSyncUtils.getState(group);
        assertEquals(GroupWindowState.IN_ANOTHER, state);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetState_HeadlessWindow() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Token token = Token.createRandom();
        SavedTabGroup group = createSavedTabGroup(token, "title1");

        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(new Token(200L, 1L));

        when(tabWindowManager.findWindowIdForTabGroup(token)).thenReturn(1);
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel headlessModel = mock(TabModel.class);
        when(headlessModel.getTabModelType()).thenReturn(TabModelType.HEADLESS);
        when(selector.getModel(false)).thenReturn(headlessModel);
        when(tabWindowManager.getTabModelSelectorById(1)).thenReturn(selector);

        @GroupWindowState int state = mSyncUtils.getState(group);
        assertEquals(GroupWindowState.HIDDEN, state);
    }

    @Test
    public void testGetState_Token_inCurrent() {
        Token token = Token.createRandom();
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.isClosing()).thenReturn(false);

        @GroupWindowState int state = mSyncUtils.getState(token);
        assertEquals(GroupWindowState.IN_CURRENT, state);
    }

    @Test
    public void testGetState_Token_inCurrentClosing() {
        Token token = Token.createRandom();
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token);
        when(mTab1.isClosing()).thenReturn(true);

        @GroupWindowState int state = mSyncUtils.getState(token);
        assertEquals(GroupWindowState.IN_CURRENT_CLOSING, state);
    }

    @Test
    public void testGetState_Token_inAnother() {
        Token token = Token.createRandom();
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());

        @GroupWindowState int state = mSyncUtils.getState(token);
        assertEquals(GroupWindowState.IN_ANOTHER, state);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetState_Token_hidden() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        Token token = Token.createRandom();
        List<Tab> tabList = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());

        when(tabWindowManager.findWindowIdForTabGroup(token)).thenReturn(1);
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel headlessModel = mock(TabModel.class);
        when(headlessModel.getTabModelType()).thenReturn(TabModelType.HEADLESS);
        when(selector.getModel(false)).thenReturn(headlessModel);
        when(tabWindowManager.getTabModelSelectorById(1)).thenReturn(selector);

        @GroupWindowState int state = mSyncUtils.getState(token);
        assertEquals(GroupWindowState.HIDDEN, state);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetSortedGroupList_incognitoMultiWindow() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        when(mTabModel.isIncognito()).thenReturn(true);
        Token group1 = Token.createRandom();
        Token group2 = Token.createRandom();
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(group1));
        when(mTabModel.getTabGroupTitle(group1)).thenReturn("Incognito Group 1");
        when(mTabModel.tabGroupExists(group1)).thenReturn(true);
        when(mTabModel.getTabsInGroup(group1)).thenReturn(List.of());
        when(mTab1.getTabGroupId()).thenReturn(group1);
        List<Tab> tabList1 = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList1.iterator());

        TabModel otherIncognitoModel = mock(TabModel.class);
        when(otherIncognitoModel.isIncognito()).thenReturn(true);
        when(otherIncognitoModel.getAllTabGroupIds()).thenReturn(Set.of(group2));
        when(otherIncognitoModel.getTabGroupTitle(group2)).thenReturn("Incognito Group 2");
        when(otherIncognitoModel.getTabsInGroup(group2)).thenReturn(List.of());
        when(otherIncognitoModel.tabGroupExists(group2)).thenReturn(true);

        TabModelSelector selector1 = mock(TabModelSelector.class);
        when(selector1.getModel(true)).thenReturn(mTabModel);
        TabModelSelector selector2 = mock(TabModelSelector.class);
        when(selector2.getModel(true)).thenReturn(otherIncognitoModel);
        when(tabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector1, selector2));

        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        state -> true, (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(2, sortedList.size());
        assertEquals("Incognito Group 1", sortedList.get(0).title);
        assertEquals("Incognito Group 2", sortedList.get(1).title);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetSortedGroupList_incognitoSingleWindow_flagDisabled() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        when(mTabModel.isIncognito()).thenReturn(true);
        Token group1 = Token.createRandom();
        Token group2 = Token.createRandom();
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(group1));
        when(mTabModel.getTabGroupTitle(group1)).thenReturn("Incognito Group 1");
        when(mTabModel.tabGroupExists(group1)).thenReturn(true);
        when(mTabModel.getTabsInGroup(group1)).thenReturn(List.of());
        when(mTab1.getTabGroupId()).thenReturn(group1);
        List<Tab> tabList1 = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList1.iterator());

        TabModel otherIncognitoModel = mock(TabModel.class);
        when(otherIncognitoModel.isIncognito()).thenReturn(true);
        when(otherIncognitoModel.getAllTabGroupIds()).thenReturn(Set.of(group2));
        when(otherIncognitoModel.getTabGroupTitle(group2)).thenReturn("Incognito Group 2");
        when(otherIncognitoModel.getTabsInGroup(group2)).thenReturn(List.of());
        when(otherIncognitoModel.tabGroupExists(group2)).thenReturn(true);

        TabModelSelector selector1 = mock(TabModelSelector.class);
        when(selector1.getModel(true)).thenReturn(mTabModel);
        TabModelSelector selector2 = mock(TabModelSelector.class);
        when(selector2.getModel(true)).thenReturn(otherIncognitoModel);
        when(tabWindowManager.getAllTabModelSelectors()).thenReturn(List.of(selector1, selector2));

        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        state -> true, (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(1, sortedList.size());
        assertEquals("Incognito Group 1", sortedList.get(0).title);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testGetSortedGroupList_incognitoSingleWindow_emptyWindowManager() {
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        when(mTabModel.isIncognito()).thenReturn(true);
        Token group1 = Token.createRandom();
        when(mTabModel.getAllTabGroupIds()).thenReturn(Set.of(group1));
        when(mTabModel.getTabGroupTitle(group1)).thenReturn("Incognito Group 1");
        when(mTabModel.tabGroupExists(group1)).thenReturn(true);
        when(mTabModel.getTabsInGroup(group1)).thenReturn(List.of());
        when(mTab1.getTabGroupId()).thenReturn(group1);
        List<Tab> tabList1 = List.of(mTab1);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList1.iterator());

        when(tabWindowManager.getAllTabModelSelectors()).thenReturn(List.of());

        List<GroupWindowInfo> sortedList =
                mSyncUtils.getSortedGroupList(
                        state -> true, (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(1, sortedList.size());
        assertEquals("Incognito Group 1", sortedList.get(0).title);
    }

    private SavedTabGroup createSavedTabGroup(Token token, String title) {
        SavedTabGroup tabGroup = new SavedTabGroup();
        tabGroup.localId = new LocalTabGroupId(token);
        tabGroup.savedTabs = new ArrayList<>();
        tabGroup.title = title;
        return tabGroup;
    }

    private boolean tabGroupSelectionPredicate(@GroupWindowState int groupWindowState) {
        return groupWindowState != GroupWindowState.IN_ANOTHER;
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testShouldShowGroupByState() {
        assertFalse(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_CURRENT_CLOSING));
        assertFalse(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.HIDDEN));
        assertTrue(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_CURRENT));
        assertFalse(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_ANOTHER));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CROSS_WINDOW_TAB_GROUP_OPERATIONS)
    public void testShouldShowGroupByState_crossWindowEnabled() {
        assertFalse(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_CURRENT_CLOSING));
        assertFalse(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.HIDDEN));
        assertTrue(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_CURRENT));
        assertTrue(GroupWindowChecker.shouldShowGroupByState(GroupWindowState.IN_ANOTHER));
    }

    @Test
    public void testGetDefaultSortedGroupList() {
        Token token1 = Token.createRandom();
        Token token2 = Token.createRandom();

        SavedTabGroup group1 = createSavedTabGroup(token1, "title1");
        group1.updateTimeMs = 100L;
        SavedTabGroup group2 = createSavedTabGroup(token2, "title2");
        group2.updateTimeMs = 200L;

        group1.localId = new LocalTabGroupId(token1);
        group2.localId = new LocalTabGroupId(token2);
        group1.savedTabs.add(new SavedTabGroupTab());
        group2.savedTabs.add(new SavedTabGroupTab());

        when(mSyncService.getAllGroupIds()).thenReturn(new String[] {"id1", "id2"});
        when(mSyncService.getGroup("id1")).thenReturn(group1);
        when(mSyncService.getGroup("id2")).thenReturn(group2);

        Tab tab2 = mock(Tab.class);
        when(tab2.getTabGroupId()).thenReturn(token2);
        List<Tab> tabList = List.of(mTab1, tab2);
        when(mTabList.iterator()).thenAnswer(invocation -> tabList.iterator());
        when(mTab1.getTabGroupId()).thenReturn(token1);

        List<GroupWindowInfo> sortedList = mSyncUtils.getDefaultSortedGroupList();

        assertEquals(2, sortedList.size());
        assertEquals("title2", sortedList.get(0).title);
        assertEquals("title1", sortedList.get(1).title);
    }
}
