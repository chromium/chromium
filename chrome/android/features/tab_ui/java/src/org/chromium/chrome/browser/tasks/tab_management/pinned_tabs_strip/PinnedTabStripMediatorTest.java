// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Size;
import android.view.View;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.List;

/** Unit tests for {@link PinnedTabStripMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PinnedTabStripMediatorTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    @Mock private GridLayoutManager mLayoutManager;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private PinnedTabStripItemContextMenuCoordinator mMenuCoordinator;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Profile mProfile;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Runnable mOnTabGroupCreation;
    @Mock private View mMockView;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabListModel mTabListModel;
    private TabListModel mPinnedTabsModelList;
    private PropertyModel mStripPropertyModel;
    private PinnedTabStripMediator mMediator;
    private TestActivity mActivity;
    private TabListItemSizeChangedObserver mTabListItemSizeChangedObserver;

    @Before
    public void setUp() {
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabGroupModelFilter.getTabModel()).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getProfile()).thenReturn(mProfile);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
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
                        mStripPropertyModel,
                        mTabGroupModelFilterSupplier,
                        mTabBookmarkerSupplier,
                        mBottomSheetController,
                        mModalDialogManager,
                        mOnTabGroupCreation);

        ArgumentCaptor<TabListItemSizeChangedObserver> observerCaptor =
                ArgumentCaptor.forClass(TabListItemSizeChangedObserver.class);
        verify(mTabListCoordinator).addTabListItemSizeChangedObserver(observerCaptor.capture());
        mTabListItemSizeChangedObserver = observerCaptor.getValue();
        when(mLayoutManager.getSpanCount()).thenReturn(2);

        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        mMediator.setContextMenuCoordinatorForTesting(mMenuCoordinator);
        verify(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
    }

    @Test
    public void testOnLongPress() {
        View view = new View(mActivity);
        int tabId = 123123;
        mMediator.onLongPress(tabId, view);
        verify(mMenuCoordinator).showMenu(any(ViewRectProvider.class), eq(tabId));
    }

    @Test
    public void testContextClickListener() {
        int tabId = 123123;
        mTabListModel.add(createTabListItem(tabId, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.size()).isEqualTo(1);

        PropertyModel model = mPinnedTabsModelList.get(0).model;
        TabActionListener listener = model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER);
        Assert.assertNotNull(listener);

        View view = new View(mActivity);
        listener.run(view, tabId, null);
        verify(mMenuCoordinator).showMenu(any(ViewRectProvider.class), eq(tabId));
    }

    @Test
    public void testUpdatePinnedTabsBar_NoUpdateWhenListsAreSame() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled(); // First scroll to populate the pinned list.

        mMediator.onScrolled(); // Second scroll should do nothing.
        assertThat(mPinnedTabsModelList.size()).isEqualTo(1);
    }

    @Test
    public void testGetVisiblePinnedTabs_NoPinnedTabs() {
        mTabListModel.add(createTabListItem(1, false));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.isEmpty()).isTrue();
    }

    @Test
    public void testGetVisiblePinnedTabs_PinnedTabsVisible() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.isEmpty()).isTrue();
    }

    @Test
    public void testGetVisiblePinnedTabs_PinnedTabsScrolledOff() {
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, false));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.size()).isEqualTo(1);
        assertThat(mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID)).isEqualTo(1);
    }

    @Test
    public void testGetVisiblePinnedTabs_PartiallyObscuredRowIsPinned() {
        // Create a scenario where the first visible row is partially obscured.
        // Tab 1 (pinned, scrolled off)
        mTabListModel.add(createTabListItem(1, true));
        // Tab 2 (pinned, scrolled off)
        mTabListModel.add(createTabListItem(2, true));
        // Tab 3 (pinned, first in the partially obscured row)
        mTabListModel.add(createTabListItem(3, true));
        // Tab 4 (pinned, second in the partially obscured row)
        mTabListModel.add(createTabListItem(4, true));
        // Tab 4 (not pinned)
        mTabListModel.add(createTabListItem(5, false));

        // Simulate the layout manager returning a view for the first visible item (Tab 3).
        when(mMockView.getBottom())
                .thenReturn(50); // Partially obscured (less than rowCoverage: 85).
        when(mLayoutManager.findViewByPosition(2)).thenReturn(mMockView);
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2); // Tab 3 is first visible
        when(mLayoutManager.getSpanCount()).thenReturn(2); // Two tabs per row

        mMediator.onScrolled();

        // Expect Tab 1, Tab 2, Tab 3 and Tab 4 to be in pinned bar.
        assertThat(mPinnedTabsModelList.size()).isEqualTo(4);
        assertThat(mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID)).isEqualTo(1);
        assertThat(mPinnedTabsModelList.get(1).model.get(TabProperties.TAB_ID)).isEqualTo(2);
        assertThat(mPinnedTabsModelList.get(2).model.get(TabProperties.TAB_ID)).isEqualTo(3);
        assertThat(mPinnedTabsModelList.get(3).model.get(TabProperties.TAB_ID)).isEqualTo(4);
    }

    @Test
    public void testUpdatePinnedTabsModel_AddNewTabs() {
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, true));
        mTabListModel.add(createTabListItem(3, true));
        mTabListModel.add(createTabListItem(4, false));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(3);
        mMediator.onScrolled();
        assertEquals(3, mPinnedTabsModelList.size());
        assertEquals(2, mStripPropertyModel.get(PinnedTabStripProperties.SCROLL_TO_POSITION));
    }

    @Test
    public void testUpdatePinnedTabsModel_RemoveTabs() {
        mTabListModel.add(createTabListItem(1, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.size()).isEqualTo(1);

        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(0);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.isEmpty()).isTrue();
    }

    @Test
    public void testUpdatePinnedTabsModel_ReplaceTabs() {
        mTabListModel.add(createTabListItem(1, true));
        mTabListModel.add(createTabListItem(2, true));
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(1);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.size()).isEqualTo(1);
        assertThat(mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID)).isEqualTo(1);

        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        mMediator.onScrolled();
        assertThat(mPinnedTabsModelList.size()).isEqualTo(2);
        assertThat(mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID)).isEqualTo(1);
        assertThat(mPinnedTabsModelList.get(1).model.get(TabProperties.TAB_ID)).isEqualTo(2);
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
    public void testOnCardSizeChanged_SpanCountChange_UpdatesPinnedTabs() {
        // Add pinned tabs to the main model.
        for (int i = 0; i < 5; i++) {
            mTabListModel.add(createTabListItem(i, true));
        }
        mTabListModel.add(createTabListItem(5, false));

        // Set initial state: first visible is item 2.
        // This means tabs 0 and 1 are pinned and scrolled off.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(2);
        mMediator.onScrolled();
        assertEquals(2, mPinnedTabsModelList.size());

        // Simulate screen size change, span count changes. Let's assume with a larger screen or
        // smaller cards, more items are visible on screen at once. If the user has scrolled to the
        // same position, the first visible item index might be higher.
        when(mLayoutManager.findFirstVisibleItemPosition()).thenReturn(3);
        mTabListItemSizeChangedObserver.onSizeChanged(3, new Size(200, 0));

        // Verify pinned tabs bar is updated. Now tabs 0, 1, 2 are pinned and scrolled off.
        assertEquals(3, mPinnedTabsModelList.size());
        assertEquals(0, mPinnedTabsModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(1, mPinnedTabsModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(2, mPinnedTabsModelList.get(2).model.get(TabProperties.TAB_ID));
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
    public void testDidSelectTab_OldIndexValid() {
        addTabToPinnedModel(1, true);
        addTabToPinnedModel(2, false);
        when(mTab2.getId()).thenReturn(2);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab2, 0, 1);

        // Verify old tab is deselected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertFalse(model1.get(TabProperties.IS_SELECTED));

        // Verify new tab is selected.
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertTrue(model2.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testDidSelectTab_NewIndexValid() {
        addTabToPinnedModel(1, false);
        addTabToPinnedModel(2, false);
        when(mTab1.getId()).thenReturn(1);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, 0, 2);

        // Verify new tab is selected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertTrue(model1.get(TabProperties.IS_SELECTED));

        // Verify other tab remains deselected.
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertFalse(model2.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testDidSelectTab_BothIndicesValid() {
        addTabToPinnedModel(1, true);
        addTabToPinnedModel(2, false);
        addTabToPinnedModel(3, false);
        when(mTab2.getId()).thenReturn(2);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab2, 0, 1);

        // Verify old tab is deselected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertFalse(model1.get(TabProperties.IS_SELECTED));

        // Verify new tab is selected.
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertTrue(model2.get(TabProperties.IS_SELECTED));

        // Verify other tab remains deselected.
        PropertyModel model3 = mPinnedTabsModelList.get(2).model;
        Assert.assertFalse(model3.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testDidSelectTab_OldIndexInvalid() {
        addTabToPinnedModel(1, false);
        addTabToPinnedModel(2, false);
        when(mTab1.getId()).thenReturn(1);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, 0, 99); // oldId=99 is not in model

        // Verify new tab is selected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertTrue(model1.get(TabProperties.IS_SELECTED));

        // Verify other tab remains deselected.
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertFalse(model2.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testDidSelectTab_NewIndexInvalid() {
        addTabToPinnedModel(1, true);
        addTabToPinnedModel(2, false);
        when(mTab3.getId()).thenReturn(3);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab3, 0, 1); // new tab with id=3 is not in model

        // Verify old tab is deselected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertFalse(model1.get(TabProperties.IS_SELECTED));

        // Verify other tab remains deselected.
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertFalse(model2.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testDidSelectTab_BothIndicesInvalid() {
        addTabToPinnedModel(1, false);
        addTabToPinnedModel(2, false);
        when(mTab3.getId()).thenReturn(3);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab3, 0, 99);

        // Verify no tabs are selected.
        PropertyModel model1 = mPinnedTabsModelList.get(0).model;
        Assert.assertFalse(model1.get(TabProperties.IS_SELECTED));
        PropertyModel model2 = mPinnedTabsModelList.get(1).model;
        Assert.assertFalse(model2.get(TabProperties.IS_SELECTED));
    }

    @Test
    public void testTabPinned_updatePinnedBar() {
        mMediator.onScrolled(); // Initial state.
        mTabModelObserverCaptor.getValue().didChangePinState(mTab1);
        // Should be called twice, once for the initial scroll and once for the pin event.
        verify(mLayoutManager, times(2)).findFirstVisibleItemPosition();
    }

    @Test
    public void testTabClosureCommitted_updatePinnedBar() {
        mMediator.onScrolled(); // Initial state.
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        // Should be called twice, once for the initial scroll and once for the closure event.
        verify(mLayoutManager, times(2)).findFirstVisibleItemPosition();
    }

    @Test
    public void testOnTabClosePending_updatePinnedBar() {
        mMediator.onScrolled(); // Initial state.
        mTabModelObserverCaptor
                .getValue()
                .onTabClosePending(List.of(mTab1), false, TabClosingSource.UNKNOWN);
        // Should be called twice, once for the initial scroll and once for the pending closure
        // event.
        verify(mLayoutManager, times(2)).findFirstVisibleItemPosition();
    }

    @Test
    public void testTabClosureUndone_updatePinnedBar_postsToUiThread() {
        mMediator.onScrolled(); // Initial state.
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);
        // The updatePinnedTabsBar() call is now posted to the UI thread.
        // We need to advance the looper to ensure the posted task is executed.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mLayoutManager, times(2)).findFirstVisibleItemPosition();
    }

    @Test
    public void testChangingTabGroupModelFilters() {
        mTabGroupModelFilterSupplier.set(mIncognitoTabGroupModelFilter);

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mIncognitoTabGroupModelFilter).addObserver(any());
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mTabListCoordinator)
                .removeTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
    }

    @Test
    public void testIsPinnedTabsBarVisible() {
        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, true);
        assertTrue(mMediator.isPinnedTabsBarVisible());

        mStripPropertyModel.set(PinnedTabStripProperties.IS_VISIBLE, false);
        assertFalse(mMediator.isPinnedTabsBarVisible());
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

    private void addTabToPinnedModel(int tabId, boolean isSelected) {
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, tabId)
                        .with(TabProperties.IS_SELECTED, isSelected)
                        .build();
        mPinnedTabsModelList.add(new ListItem(TabProperties.UiType.TAB, model));
    }
}
