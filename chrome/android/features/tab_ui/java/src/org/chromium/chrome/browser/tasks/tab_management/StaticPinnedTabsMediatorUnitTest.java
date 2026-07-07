// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link StaticPinnedTabsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StaticPinnedTabsMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private Runnable mOnVisibilityChanged;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabListModel mMainModelList;
    private TabListModel mPinnedModelList;
    private StaticPinnedTabsMediator mMediator;
    private TabModelObserver mTabModelObserver;

    @Before
    public void setUp() {
        mMainModelList = new TabListModel();
        mPinnedModelList = new TabListModel();

        // Setup mock tabs.
        when(mTab1.getId()).thenReturn(1);
        when(mTab1.getIsPinned()).thenReturn(true);

        when(mTab2.getId()).thenReturn(2);
        when(mTab2.getIsPinned()).thenReturn(false);

        when(mTab3.getId()).thenReturn(3);
        when(mTab3.getIsPinned()).thenReturn(true);

        // Setup mock TabModel indices.
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(mTab3);

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(mTab3)).thenReturn(2);

        when(mTabModel.getTabById(1)).thenReturn(mTab1);
        when(mTabModel.getTabById(2)).thenReturn(mTab2);
        when(mTabModel.getTabById(3)).thenReturn(mTab3);

        mMediator =
                new StaticPinnedTabsMediator(
                        mTabModel, mMainModelList, mPinnedModelList, mOnVisibilityChanged);

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        mTabModelObserver = mTabModelObserverCaptor.getValue();
    }

    @Test
    public void testInitialMirroring() {
        // Clear lists.
        mMainModelList.clear();
        mPinnedModelList.clear();

        // Create new items.
        ListItem item1 = createTabListItem(mTab1);
        ListItem item2 = createTabListItem(mTab2);
        ListItem item3 = createTabListItem(mTab3);

        // Add them to main list before mediator is created.
        mMainModelList.add(item1);
        mMainModelList.add(item2);
        mMainModelList.add(item3);

        // Create mediator with pre-populated main list.
        StaticPinnedTabsMediator initialMediator =
                new StaticPinnedTabsMediator(
                        mTabModel, mMainModelList, mPinnedModelList, mOnVisibilityChanged);

        // Verify pre-populated items are mirrored.
        assertEquals(2, mPinnedModelList.size());
        assertEquals(item1, mPinnedModelList.get(0));
        assertEquals(item3, mPinnedModelList.get(1));
    }

    @Test
    public void testItemInsertedAndRemovedFromMainList() {
        // Adding tabs to main list should trigger auto-mirroring.
        ListItem item1 = addTabToMainList(mTab1); // Pinned.
        ListItem item2 = addTabToMainList(mTab2); // Regular.

        // Verify that only the pinned tab is mirrored.
        assertEquals(1, mPinnedModelList.size());
        assertEquals(item1, mPinnedModelList.get(0));

        // Remove item1 from main list. ListObserver should remove it from pinned.
        mMainModelList.removeAt(0);
        assertTrue(mPinnedModelList.isEmpty());
    }

    @Test
    public void testDidChangePinState() {
        ListItem item2 = addTabToMainList(mTab2);
        assertTrue(mPinnedModelList.isEmpty());

        // Pin mTab2 in the backend.
        when(mTab2.getIsPinned()).thenReturn(true);
        item2.model.set(TabProperties.IS_PINNED, true);

        // Notify mediator of pin state change.
        mTabModelObserver.didChangePinState(mTab2);

        // Verify it is mirrored.
        assertEquals(1, mPinnedModelList.size());
        assertEquals(item2, mPinnedModelList.get(0));

        // Unpin mTab2 in the backend.
        when(mTab2.getIsPinned()).thenReturn(false);
        item2.model.set(TabProperties.IS_PINNED, false);

        // Notify mediator.
        mTabModelObserver.didChangePinState(mTab2);

        // Verify it is removed.
        assertTrue(mPinnedModelList.isEmpty());
    }

    @Test
    public void testDidChangePinStateOutOrOrder() {
        // Clear both models to start fresh.
        mMainModelList.clear();
        mPinnedModelList.clear();

        // Add Tab 3 (pinned) first.
        ListItem item3 = addTabToMainList(mTab3);
        assertEquals(1, mPinnedModelList.size());
        assertEquals(item3, mPinnedModelList.get(0));

        // Add Tab 1 (pinned) second. It should be inserted at index 0 because
        // it has a smaller index in the TabModel than Tab 3.
        ListItem item1 = addTabToMainList(mTab1);
        assertEquals(2, mPinnedModelList.size());
        assertEquals(item1, mPinnedModelList.get(0));
        assertEquals(item3, mPinnedModelList.get(1));
    }

    @Test
    public void testTabRemovedAndClosed() {
        ListItem item1 = addTabToMainList(mTab1);
        assertEquals(1, mPinnedModelList.size());

        // Simulate tab removal in both the model list and backend.
        mMainModelList.removeAt(0);
        mTabModelObserver.tabRemoved(mTab1);
        assertTrue(mPinnedModelList.isEmpty());

        // Add back and test closure.
        ListItem item1Readded = addTabToMainList(mTab1);
        assertEquals(1, mPinnedModelList.size());

        mMainModelList.removeAt(0);
        mTabModelObserver.tabClosureCommitted(mTab1);
        assertTrue(mPinnedModelList.isEmpty());
    }

    @Test
    public void testMainListCleared() {
        ListItem item1 = addTabToMainList(mTab1);
        ListItem item3 = addTabToMainList(mTab3);
        assertEquals(2, mPinnedModelList.size());

        // Clear main list.
        mMainModelList.clear();

        // Pinned list must be cleared.
        assertTrue(mPinnedModelList.isEmpty());
    }

    @Test
    public void testOnItemMoved() {
        ListItem item1 = addTabToMainList(mTab1);
        ListItem item3 = addTabToMainList(mTab3);
        assertEquals(2, mPinnedModelList.size());
        assertEquals(item1, mPinnedModelList.get(0));
        assertEquals(item3, mPinnedModelList.get(1));

        // Move items in main list.
        mMainModelList.move(0, 1);

        // Verify move is mirrored.
        assertEquals(item3, mPinnedModelList.get(0));
        assertEquals(item1, mPinnedModelList.get(1));
    }

    private ListItem createTabListItem(Tab tab) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tab.getId())
                        .with(TabProperties.IS_PINNED, tab.getIsPinned())
                        .with(CARD_TYPE, TAB)
                        .build();
        return new ListItem(TabProperties.UiType.TAB, model);
    }

    private ListItem addTabToMainList(Tab tab) {
        ListItem item = createTabListItem(tab);
        mMainModelList.add(item);
        return item;
    }
}
