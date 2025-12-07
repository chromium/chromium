// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link GroupWindowChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupWindowCheckerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mSyncService;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabModel mTabModel;
    @Spy private TabList mTabList;
    @Mock private Tab mTab1;
    private GroupWindowChecker mSyncUtils;

    @Before
    public void setUp() {
        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabList);

        mSyncUtils = new GroupWindowChecker(mSyncService, mFilter);
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

        List<SavedTabGroup> sortedList =
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

        List<SavedTabGroup> sortedList =
                mSyncUtils.getSortedGroupList(
                        this::tabGroupSelectionPredicate,
                        (g1, g2) -> g1.title.compareToIgnoreCase(g2.title));

        assertEquals(1, sortedList.size());
        assertEquals("title1", sortedList.get(0).title);
    }

    @Test
    public void testGetSortedGroupList_empty() {
        when(mSyncService.getAllGroupIds()).thenReturn(new String[] {});
        List<SavedTabGroup> sortedList =
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
}
