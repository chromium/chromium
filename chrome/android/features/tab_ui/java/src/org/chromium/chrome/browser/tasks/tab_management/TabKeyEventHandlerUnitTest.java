// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.KeyEvent.KEYCODE_PAGE_DOWN;
import static android.view.KeyEvent.KEYCODE_PAGE_UP;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Unit tests for {@link TabKeyEventHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabKeyEventHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;

    private int mNextTabId = 2748;
    private int mTabCount;

    @Before
    public void setUp() {
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
    }

    @Test
    public void testMoveForwardSingleTab() {
        addTab();
        Tab tab1 = addTab();

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab1.getId(), KEYCODE_PAGE_UP),
                mTabGroupModelFilter,
                /* moveSingleTab= */ true);

        verify(mTabModel).moveTab(tab1.getId(), 0);
    }

    @Test
    public void testMoveBackwardSingleTab() {
        Tab tab0 = addTab();
        addTab();

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab0.getId(), KEYCODE_PAGE_DOWN),
                mTabGroupModelFilter,
                /* moveSingleTab= */ true);

        verify(mTabModel).moveTab(tab0.getId(), 1);
    }

    @Test
    public void testMoveForwardGroupedTabOutOfGroup() {
        addTab();
        List<Tab> group = addTabGroup();
        Tab tab1 = group.get(0);

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab1.getId(), KEYCODE_PAGE_UP),
                mTabGroupModelFilter,
                /* moveSingleTab= */ true);

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    public void testMoveBackwardGroupTabOutOfGroup() {
        List<Tab> group = addTabGroup();
        Tab tab1 = group.get(1);
        addTab();

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab1.getId(), KEYCODE_PAGE_DOWN),
                mTabGroupModelFilter,
                /* moveSingleTab= */ true);

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    public void testMoveBackwardSingleTabWithGroup() {
        Tab tab = addTab();
        addTabGroup();

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab.getId(), KEYCODE_PAGE_DOWN),
                mTabGroupModelFilter,
                /* moveSingleTab= */ false);

        verify(mTabGroupModelFilter).moveRelatedTabs(tab.getId(), 2);
    }

    @Test
    public void testMoveForwardGroupWithGroup() {
        addTabGroup();
        Tab tab = addTab();

        TabKeyEventHandler.onPageKeyEvent(
                new TabKeyEventData(tab.getId(), KEYCODE_PAGE_UP),
                mTabGroupModelFilter,
                /* moveSingleTab= */ false);

        verify(mTabGroupModelFilter).moveRelatedTabs(tab.getId(), 0);
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

    private List<Tab> addTabGroup() {
        Tab tab0 = addTab();
        Tab tab1 = addTab();

        Token tabGroupId = new Token(tab0.getId(), tab1.getId());
        when(tab0.getTabGroupId()).thenReturn(tabGroupId);
        when(tab1.getTabGroupId()).thenReturn(tabGroupId);

        List<Tab> tabs = List.of(tab0, tab1);
        when(mTabGroupModelFilter.getRelatedTabList(tab0.getId())).thenReturn(tabs);
        when(mTabGroupModelFilter.getRelatedTabList(tab1.getId())).thenReturn(tabs);
        return tabs;
    }
}
