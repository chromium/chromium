// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link MoveTabUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MoveTabUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    private @TabId int mNextTabId = 123;
    private int mTabCount;

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_NoOperation() {
        addTabs(1); // 0
        addTabGroup(3, new Token(1L, 2L)); // 1 2 3
        addTabs(1); // 4
        addTabGroup(2, new Token(2L, 1L)); // 5 6
        addTabGroup(1, new Token(3L, 2L)); // 7
        addTab(); // 8

        int curIndex = 0;
        Tab tab = mTabModel.getTabAt(curIndex);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 0);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 1);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 2);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());

        curIndex = 4;
        tab = mTabModel.getTabAt(curIndex);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 3);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 2);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());

        curIndex = 7;
        tab = mTabModel.getTabAt(curIndex);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 6);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab, /* requestedIndex= */ 8);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_LowerIndex() {
        addTabs(3); // 0 1 2
        int index = 2;
        int requestedIndex = 0;
        Tab tab2 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab2, requestedIndex);
        verify(mTabModel).moveTab(tab2.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_LowerIndex_TabGroupOf1() {
        addTabGroup(1, new Token(1L, 2L)); // 0
        addTabs(1); // 1
        int index = 1;
        int requestedIndex = 0;
        Tab tab1 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab1, requestedIndex);
        verify(mTabModel).moveTab(tab1.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_LowerIndex_IndexOverlapsWithTabGroup() {
        addTabGroup(3, new Token(1L, 2L)); // 0 1 2
        addTab(); // 3

        int index = 3;
        int requestedIndex = 0;
        Tab tab3 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab3, requestedIndex);
        verify(mTabModel).moveTab(tab3.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTabInsideTabGroup_LowerIndex() {
        addTabGroup(3, new Token(1L, 2L)); // 0 1 2
        int index = 2;
        int requestedIndex = 0;
        Tab tab2 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab2, requestedIndex);
        verify(mTabModel).moveTab(tab2.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTabInsideTabGroup_LowerIndex_IndexOutOfTabGroup() {
        addTabs(2); // 0 1
        addTabGroup(3, new Token(1L, 2L)); // 2 3 4
        int index = 3;
        int requestedIndex = 1;
        Tab tab3 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab3, requestedIndex);
        verify(mTabModel).moveTab(tab3.getId(), 2); // First index of tab group.
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_HigherIndex() {
        addTabs(3); // 0 1 2
        int index = 0;
        int requestedIndex = 1;
        Tab tab2 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab2, requestedIndex);
        verify(mTabModel).moveTab(tab2.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_HigherIndex_TabGroupOf1() {
        addTabs(1); // 0
        addTabGroup(1, new Token(1L, 2L)); // 1
        int index = 0;
        int requestedIndex = 1;
        Tab tab1 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab1, requestedIndex);
        verify(mTabModel).moveTab(tab1.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTab_HigherIndex_IndexOverlapsWithTabGroup() {
        addTabs(2); // 0 1
        addTabGroup(4, new Token(1L, 2L)); // 2 3 4 5
        addTab(); // 6

        int index = 1;
        int requestedIndex = 4;
        Tab tab0 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab0, requestedIndex);
        verify(mTabModel).moveTab(tab0.getId(), requestedIndex + 1);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTabInsideTabGroup_HigherIndex() {
        addTabGroup(5, new Token(1L, 2L)); // 0 1 2 3 4
        int index = 1;
        int requestedIndex = 3;
        Tab tab2 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab2, requestedIndex);
        verify(mTabModel).moveTab(tab2.getId(), requestedIndex);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testMoveSingleTabInsideTabGroup_HigherIndex_IndexOutOfTabGroup() {
        addTabGroup(3, new Token(1L, 2L)); // 0 1 2
        addTabs(2); // 3 4
        int index = 1;
        int requestedIndex = 4;
        Tab tab2 = mTabModel.getTabAt(index);
        MoveTabUtils.moveSingleTab(mTabModel, mTabGroupModelFilter, tab2, requestedIndex);
        verify(mTabModel).moveTab(tab2.getId(), 2); // Last index of tab group.
    }

    @Test
    public void testMoveTabGroup_NoOperation() {
        Token groupId0 = new Token(1L, 2L);
        Token groupId1 = new Token(2L, 3L);
        Token groupId2 = new Token(3L, 4L);
        addTabGroup(4, groupId0); // 0 1 2 3
        addTabGroup(5, groupId1); // 4 5 6 7 8
        addTabGroup(4, groupId2); // 9 10 11 12

        // Destination Tab is in the same group
        int requestedIndex = 0;
        for (int i = 0; i < 4; i++) {
            MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId0, requestedIndex++);
            verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        }

        // Adjacent tab groups
        // Group0:
        requestedIndex = 4;
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId0, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        requestedIndex = 5;
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId0, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        requestedIndex = 6;
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId0, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());

        // Group2:
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId2, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        requestedIndex = 7;
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId2, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
        requestedIndex = 8;
        MoveTabUtils.moveTabGroup(mTabModel, mTabGroupModelFilter, groupId2, requestedIndex);
        verify(mTabGroupModelFilter, never()).moveRelatedTabs(anyInt(), anyInt());
    }

    @Test
    public void testMoveTabGroup_SimpleMove() {
        Token sourceTabGroupId = new Token(1L, 2L);
        addTab(); // 0
        @TabId int firstTabIdInGroup = addTabGroup(3, sourceTabGroupId); // 1 2 3
        addTab(); // 4

        int requestedIndex = 0;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, requestedIndex);

        requestedIndex = 4;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, requestedIndex);
    }

    @Test
    public void testMoveTabGroup_IndexOverlapsWithTabGroup() {
        Token sourceTabGroupId = new Token(1L, 2L);
        addTabGroup(4, new Token(3L, 2L)); // 0 1 2 3
        @TabId int firstTabIdInGroup = addTabGroup(2, sourceTabGroupId); // 4 5
        addTabGroup(4, new Token(4L, 5L)); // 6 7 8 9

        int requestedIndex = 0;
        int validIndex = 0;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, validIndex);

        requestedIndex = 1;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter, times(2)).moveRelatedTabs(firstTabIdInGroup, validIndex);

        requestedIndex = 8;
        validIndex = 9;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, validIndex);

        requestedIndex = 9;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter, times(2)).moveRelatedTabs(firstTabIdInGroup, validIndex);
    }

    @Test
    public void testMoveTabGroup_TabGroupOf1() {
        Token sourceTabGroupId = new Token(1L, 2L);
        addTabGroup(1, new Token(3L, 2L)); // 0
        @TabId int firstTabIdInGroup = addTabGroup(3, sourceTabGroupId); // 1 2 3
        addTabGroup(1, new Token(4L, 5L)); // 4

        int requestedIndex = 0;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, requestedIndex);

        requestedIndex = 4;
        MoveTabUtils.moveTabGroup(
                mTabModel, mTabGroupModelFilter, sourceTabGroupId, requestedIndex);
        verify(mTabGroupModelFilter).moveRelatedTabs(firstTabIdInGroup, requestedIndex);
    }

    private void addTabs(int n) {
        for (int i = 0; i < n; i++) addTab();
    }

    private @TabId int addTabGroup(int n, Token tabGroupId) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            Tab tab = addTab();
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            tabs.add(tab);
        }
        when(mTabGroupModelFilter.getTabsInGroup(tabGroupId)).thenReturn(tabs);
        return tabs.get(0).getId();
    }

    private Tab addTab() {
        Tab tab = mock(Tab.class);
        int tabId = mNextTabId++;
        when(tab.getId()).thenReturn(tabId);
        when(mTabModel.getTabById(tabId)).thenReturn(tab);
        int index = mTabCount++;
        when(mTabModel.indexOf(tab)).thenReturn(index);
        when(mTabModel.getTabAt(index)).thenReturn(tab);
        when(mTabGroupModelFilter.getRelatedTabList(tabId)).thenReturn(List.of(tab));
        return tab;
    }
}
