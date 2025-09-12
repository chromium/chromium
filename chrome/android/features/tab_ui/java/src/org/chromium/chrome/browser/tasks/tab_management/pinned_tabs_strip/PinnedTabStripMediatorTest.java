// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Size;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PinnedTabStripMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private GridLayoutManager mLayoutManager;
    @Mock private TabListCoordinator mTabListCoordinator;
    private TabListModel mTabListModel;
    private TabListModel mPinnedTabsModelList;
    private PropertyModel mStripPropertyModel;
    private PinnedTabStripMediator mMediator;
    private TestActivity mActivity;
    private TabListItemSizeChangedObserver mTabListItemSizeChangedObserver;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    void onActivity(TestActivity activity) {
        mActivity = activity;
        mTabListModel = new TabListModel();
        mPinnedTabsModelList = new TabListModel();
        mStripPropertyModel =
                new PropertyModel.Builder(PinnedTabStripProperties.ALL_KEYS)
                        .with(PinnedTabStripProperties.IS_VISIBLE, false)
                        .with(PinnedTabStripProperties.SCROLL_TO_POSITION, -1)
                        .build();

        mMediator =
                new PinnedTabStripMediator(
                        activity,
                        mLayoutManager,
                        mTabListCoordinator,
                        mTabListModel,
                        mPinnedTabsModelList,
                        mStripPropertyModel);

        ArgumentCaptor<TabListItemSizeChangedObserver> observerCaptor =
                ArgumentCaptor.forClass(TabListItemSizeChangedObserver.class);
        verify(mTabListCoordinator).addTabListItemSizeChangedObserver(observerCaptor.capture());
        mTabListItemSizeChangedObserver = observerCaptor.getValue();
        when(mLayoutManager.getSpanCount()).thenReturn(2);
    }

    @Test
    public void testUpdatePinnedTabsBar_NoUpdateWhenListsAreSame() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled(); // First scroll to populate the pinned list.

        mMediator.onScrolled(); // Second scroll should do nothing.
        assert mPinnedTabsModelList.size() == 1;
    }

    @Test
    public void testGetVisiblePinnedTabs_NoPinnedTabs() {
        mTabListModel.add(createTabListItem(1, false));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.isEmpty();
    }

    @Test
    public void testGetVisiblePinnedTabs_PinnedTabsVisible() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.isEmpty();
    }

    @Test
    public void testGetVisiblePinnedTabs_PinnedTabsScrolledOff() {
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, false));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.size() == 1;
        assert mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID) == 1;
    }

    @Test
    public void testUpdatePinnedTabsModel_AddNewTabs() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertEquals(1, mPinnedTabsModelList.size());
        assertEquals(0, mStripPropertyModel.get(PinnedTabStripProperties.SCROLL_TO_POSITION));
    }

    @Test
    public void testUpdatePinnedTabsModel_RemoveTabs() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.size() == 1;

        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.isEmpty();
    }

    @Test
    public void testUpdatePinnedTabsModel_ReplaceTabs() {
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.size() == 1;
        assert mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID) == 1;

        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        mMediator.onScrolled();
        assert mPinnedTabsModelList.size() == 2;
        assert mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID) == 1;
        assert mPinnedTabsModelList.get(1).model.get(TabProperties.TAB_ID) == 2;
    }

    @Test
    public void testUpdateStripVisibility_BecomesVisible() {
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, false);
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertTrue(mStripPropertyModel.get(PinnedTabStripProperties.IS_VISIBLE));
    }

    @Test
    public void testUpdateStripVisibility_BecomesHidden() {
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled(); // Becomes visible
        assertTrue(mStripPropertyModel.get(PinnedTabStripProperties.IS_VISIBLE));

        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        mMediator.onScrolled(); // Becomes hidden
        assertFalse(mStripPropertyModel.get(PinnedTabStripProperties.IS_VISIBLE));
    }

    @Test
    public void testUpdateStripVisibility_StaysVisible() {
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertTrue(mStripPropertyModel.get(PinnedTabStripProperties.IS_VISIBLE));
    }

    @Test
    public void testResizePinnedTabCards_ClampedToMinWidth() {
        // Set an initial size.
        final int cardWidth = 150;
        final int spanCount = 1;
        mTabListItemSizeChangedObserver.onSizeChanged(spanCount, new Size(cardWidth, 0));

        // Add enough pinned tabs to trigger resizing below the minimum width.
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, true));
        mTabListModel.add(createTabListItem(3, true));
        mTabListModel.add(createTabListItem(4, true));
        mTabListModel.add(createTabListItem(5, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(5);
        mMediator.onScrolled();

        // Verify the GRID_CARD_SIZE is clamped to the minimum width.
        assertEquals(5, mPinnedTabsModelList.size());
        PropertyModel model = mPinnedTabsModelList.get(0).model;
        Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
        int minWidth =
                PinnedTabStripUtils.getMinAllowedWidthForPinTabStripItemPx(
                        mActivity.getResources());
        assertEquals(minWidth, cardSize.getWidth());
    }

    @Test
    public void testResizePinnedTabCards_VaryingFirstVisiblePosition() {
        // Set an initial size and span count.
        final int cardWidth = 400;
        final int delta = 16;
        final int spanCount = 2;
        mTabListItemSizeChangedObserver.onSizeChanged(spanCount, new Size(cardWidth, 0));

        // Add a large number of pinned tabs to the main model.
        final int totalPinnedTabs = 12;
        for (int i = 0; i < totalPinnedTabs; i++) {
            mTabListModel.add(createTabListItem(i, true));
        }

        // Iterate through different numbers of visible pinned tabs by changing the first visible
        // position.
        for (int i = 1; i <= totalPinnedTabs; i++) {
            when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(i);
            mMediator.onScrolled();

            // Verify the GRID_CARD_SIZE has shrunk correctly based on the number of tabs.
            assertEquals(i, mPinnedTabsModelList.size());
            PropertyModel model = mPinnedTabsModelList.get(0).model;
            Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
            int expectedWidth =
                    Math.round(
                            ((cardWidth - delta)
                                    * PinnedTabStripUtils.getWidthPercentageMultiplier(
                                            mActivity.getResources(), mLayoutManager, i)));
            int minWidth =
                    PinnedTabStripUtils.getMinAllowedWidthForPinTabStripItemPx(
                            mActivity.getResources());
            expectedWidth = Math.max(minWidth, expectedWidth);

            assertEquals(
                    "Failed for firstVisiblePosition: " + i, expectedWidth, cardSize.getWidth());
        }
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mTabListCoordinator)
                .removeTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
    }

    private ListItem createTabListItem(int id, boolean isPinned) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, id)
                        .with(TabProperties.IS_PINNED, isPinned)
                        .with(
                                TabListModel.CardProperties.CARD_TYPE,
                                TabListModel.CardProperties.ModelType.TAB)
                        .build();
        return new ListItem(0, model);
    }
}
