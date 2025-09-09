// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.util.Size;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
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
    private TabListModel mTabListModel;
    private TabListModel mPinnedTabsModelList;
    private PropertyModel mStripPropertyModel;
    private PinnedTabStripMediator mMediator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    void onActivity(TestActivity activity) {
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
                        mTabListModel,
                        mPinnedTabsModelList,
                        mStripPropertyModel);
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
    public void testOnSizeChanged_setsGridCardSize() {
        // Set up the size and trigger the size change observer.
        final int cardWidth = 200;
        final int cardHeight = 300;
        final int delta = 16;
        mMediator.onSizeChanged(new Size(cardWidth, cardHeight));

        // Add a pinned tab and simulate it being scrolled off-screen.
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();

        // Verify the GRID_CARD_SIZE is set correctly.
        assertEquals(1, mPinnedTabsModelList.size());
        PropertyModel model = mPinnedTabsModelList.get(0).model;
        Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
        assertEquals(cardWidth - delta, cardSize.getWidth());
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
