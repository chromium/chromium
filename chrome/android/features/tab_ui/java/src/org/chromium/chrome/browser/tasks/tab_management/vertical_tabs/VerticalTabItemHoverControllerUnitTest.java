// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabItemHoverController.TabHoverListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabItemHoverController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabItemHoverControllerUnitTest {

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int PINNED_TAB_ID = 4;
    private static final int ROOT_VIEW_HEIGHT_PX = 1000;
    private static final int EXPANDED_CONTAINER_WIDTH_PX = 240;
    private static final int COLLAPSED_CONTAINER_WIDTH_PX = 76;
    private static final int PINNED_TAB_VIEW_HEIGHT_PX = 40;
    private static final int REGULAR_TAB_VIEW_HEIGHT_PX = 48;
    private static final int HOVER_CARD_VIEW_HEIGHT_PX = 200;
    private static final int TAB_VIEW_Y_PX = 100;
    private static final int PINNED_TAB_VIEW_X_PX = 20;
    private static final int PINNED_TAB_VIEW_Y_PX = 30;
    private static final Token GROUP_ID_1 = new Token(1L, 2L);
    private static final Token GROUP_ID_2 = new Token(3L, 4L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private TabContentManager mTabContentManager;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mPinnedTab;

    @Captor private ArgumentCaptor<Callback<Long>> mMemoryUsageCallbackCaptor;

    private final SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createMonotonic();

    private VerticalTabRailLayout mContainerView;
    private VerticalTabListRecyclerView mRecyclerView;
    private GridLayoutManager mPinnedLayoutManager;
    private ViewGroup mRootView;
    private View mTabView1;
    private View mTabView2;
    private View mPinnedTabView;
    private View mGroupHeaderView;
    private View mGroupHeaderView2;
    private ViewStub mTabHoverCardViewStub;
    private ViewStub mTabGroupHoverCardViewStub;
    private VerticalTabItemHoverController mController;
    private float mCardShadowOffset;
    private float mBackgroundInset;
    private float mHoverCardMarginToRail;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCardShadowOffset = activity.getResources().getDimension(R.dimen.popup_menu_shadow_length);
        mBackgroundInset =
                activity.getResources().getDimension(R.dimen.vertical_tab_item_touch_target_inset);
        mHoverCardMarginToRail =
                activity.getResources()
                        .getDimension(R.dimen.vertical_tab_hover_card_margin_to_rail);

        // The hover cards are inflated into the root view, so it is their parent view.
        mRootView = new FrameLayout(activity);
        activity.setContentView(
                mRootView, new LayoutParams(LayoutParams.MATCH_PARENT, ROOT_VIEW_HEIGHT_PX));
        mContainerView =
                (VerticalTabRailLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.vertical_tab_layout,
                                        mRootView,
                                        /* attachToRoot= */ false);
        mRootView.addView(
                mContainerView,
                new LayoutParams(COLLAPSED_CONTAINER_WIDTH_PX, LayoutParams.MATCH_PARENT));
        mRecyclerView = mContainerView.getRecyclerView();
        mRecyclerView.setLayoutManager(new LinearLayoutManager(activity));
        mPinnedLayoutManager = new GridLayoutManager(activity, /* spanCount= */ 2);
        mContainerView.getPinnedTabsRecyclerView().setLayoutManager(mPinnedLayoutManager);
        mTabView1 = addItemView(/* x= */ 0, TAB_VIEW_Y_PX, REGULAR_TAB_VIEW_HEIGHT_PX);
        mTabView2 =
                addItemView(
                        /* x= */ 0,
                        TAB_VIEW_Y_PX + REGULAR_TAB_VIEW_HEIGHT_PX,
                        REGULAR_TAB_VIEW_HEIGHT_PX);
        mPinnedTabView =
                addItemView(PINNED_TAB_VIEW_X_PX, PINNED_TAB_VIEW_Y_PX, PINNED_TAB_VIEW_HEIGHT_PX);
        mGroupHeaderView =
                addItemView(
                        /* x= */ 0,
                        TAB_VIEW_Y_PX + 2 * REGULAR_TAB_VIEW_HEIGHT_PX,
                        REGULAR_TAB_VIEW_HEIGHT_PX);
        mGroupHeaderView2 =
                addItemView(
                        /* x= */ 0,
                        TAB_VIEW_Y_PX + 3 * REGULAR_TAB_VIEW_HEIGHT_PX,
                        REGULAR_TAB_VIEW_HEIGHT_PX);
        mTabHoverCardViewStub = new ViewStub(activity, R.layout.tab_hover_card_holder);
        mRootView.addView(
                mTabHoverCardViewStub,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        mTabGroupHoverCardViewStub = new ViewStub(activity, R.layout.tab_group_hover_card_holder);
        mRootView.addView(
                mTabGroupHoverCardViewStub,
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        RobolectricUtil.runAllBackgroundAndUi();

        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        when(mTabContentManagerSupplier.get()).thenReturn(mTabContentManager);
        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mPinnedTab.getId()).thenReturn(PINNED_TAB_ID);
        when(mPinnedTab.getIsPinned()).thenReturn(true);
        when(mTab1.getTitle()).thenReturn("Tab 1");
        when(mTab2.getTitle()).thenReturn("Tab 2");
        when(mTab1.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab2.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModelSelector.getTabById(PINNED_TAB_ID)).thenReturn(mPinnedTab);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getTabsInGroup(GROUP_ID_1)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabsInGroup(GROUP_ID_2)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(GROUP_ID_1)).thenReturn("Group 1");
        when(mTabModel.getTabGroupTitle(GROUP_ID_2)).thenReturn("Group 2");

        mController =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        /* isContextMenuShowingSupplier= */ null);
    }

    // =========================================================================================
    // Show & Hide
    // =========================================================================================

    @Test
    public void testShowAndHide() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTabHoverCardShowing("Tab 1");

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        assertEquals(View.GONE, getTabHoverCardView().getVisibility());
    }

    @Test
    public void testSelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertNull(getTabHoverCardView());
    }

    @Test
    public void testContextMenuShowing_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> true);

        TabHoverListener listener = controller.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertNull(getTabHoverCardView());
    }

    // =========================================================================================
    // Delay
    // =========================================================================================

    @Test
    public void testGetHoverCardDelay() {
        // Collapsed rail width (76dp) -> 300ms
        setContainerWidth(VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        assertEquals(TabHoverCardView.MIN_HOVER_CARD_DELAY_MS, mController.getHoverCardDelay());

        // Expanded rail width (240dp) -> 800ms
        setContainerWidth(VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP);
        assertEquals(TabHoverCardView.MAX_HOVER_CARD_DELAY_MS, mController.getHoverCardDelay());
    }

    @Test
    public void testDelayedShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(
                TabHoverCardView.MIN_HOVER_CARD_DELAY_MS - 100, TimeUnit.MILLISECONDS);
        assertNull(getTabHoverCardView());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabHoverCardShowing("Tab 1");
    }

    @Test
    public void testExitBeforeDelay_CancelsShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Hover exit before delay expires
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Running remaining delayed tasks should not trigger show()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(getTabHoverCardView());
    }

    // =========================================================================================
    // Scrubbing Between Tabs
    // =========================================================================================

    @Test
    public void testScrubbing_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover tab 1 and wait for delay
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabHoverCardShowing("Tab 1");

        // Exit tab 1 hover (records exit time)
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Hover tab 2 within 300 ms buffer
        listener.onTabHoverStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);

        // Should show tab 2 immediately without needing ShadowLooper delay task flush
        assertTabHoverCardShowing("Tab 2");
    }

    @Test
    public void testScrubbing_EnterBeforeExit_HidesBeforeShowingTab2() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        // Tab 1 is currently hovered and showing.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTabHoverCardShowing("Tab 1");

        // Scrubbing: Tab 2 enters BEFORE Tab 1 exits (due to ViewGroup dispatch order).
        listener.onTabHoverStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);
        assertTabHoverCardShowing("Tab 2");

        // Tab 1 exits subsequently.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Tab 2 should still be showing.
        assertTabHoverCardShowing("Tab 2");
    }

    // =========================================================================================
    // Keyboard Focus
    // =========================================================================================

    @Test
    public void testKeyboardFocus_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        focusView(mTabView1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Should show immediately for keyboard focus without needing ShadowLooper delay task flush.
        assertTabHoverCardShowing("Tab 1");
    }

    @Test
    public void testKeyboardFocus_FocusLost_HidesHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        focusView(mTabView1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        assertTabHoverCardShowing("Tab 1");

        // Focus lost triggers hide.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        assertEquals(View.GONE, getTabHoverCardView().getVisibility());
    }

    @Test
    public void testKeyboardFocus_SelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);
        focusView(mTabView1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        assertNull(getTabHoverCardView());
    }

    // =========================================================================================
    // Repositions
    // =========================================================================================

    @Test
    public void testCardHeightChange_RepositionsHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTab1).getMemoryUsageBytes(mMemoryUsageCallbackCaptor.capture());
        TabHoverCardView tabHoverCardView = getTabHoverCardView();
        // Move the card so that repositioning it is observable.
        tabHoverCardView.setX(0f);
        tabHoverCardView.setY(0f);

        // When height changes (e.g. due to memory usage visibility):
        mMemoryUsageCallbackCaptor.getValue().onResult(1024L);

        float[] expectedPosition =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        tabHoverCardView,
                        /* shouldShowCardBelowTab= */ false);
        assertEquals(expectedPosition[0], tabHoverCardView.getX(), 0.01f);
        assertEquals(expectedPosition[1], tabHoverCardView.getY(), 0.01f);
    }

    // =========================================================================================
    // Get Position For Regular Tabs
    // =========================================================================================

    @Test
    public void testGetHoverCardPosition_RegularTab_Collapsed() {
        setContainerWidth(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ false);

        assertEquals(2, position.length);
        assertEquals(
                COLLAPSED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX + mBackgroundInset - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_RegularTab_Expanded() {
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ false);

        assertEquals(2, position.length);
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX + mBackgroundInset - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_ExceedsRootHeight_AlignsWithWindowBottom() {
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);
        // Position the tab view near the bottom of the window so that the hover card extends
        // beyond the root view height (900 + 188 > 1000 - 4).
        moveView(mTabView1, /* x= */ 0, /* y= */ 900);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ false);

        assertEquals(2, position.length);
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        // The hover card should align with the bottom of the window leaving a hoverCardMargin:
        // hoverCardY = parentHeight - hoverCardMargin - hoverCardHeight + cardShadowOffset.
        float expectedY =
                ROOT_VIEW_HEIGHT_PX
                        - mHoverCardMarginToRail
                        - HOVER_CARD_VIEW_HEIGHT_PX
                        + mCardShadowOffset;
        assertEquals(expectedY, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_RegularTab_VerySmallWindow_ClampsToTop() {
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);
        // Window height 180, tab at Y = 50:
        // Window bottom alignment shifts visible card top above 0 (180 - 4 - 188 = -12).
        // Clamps visibleY to 0, so hoverCardY = 0 - cardShadowOffset = -cardShadowOffset.
        setRootViewHeight(180);
        moveView(mTabView1, /* x= */ 0, /* y= */ 50);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ false);

        assertEquals(2, position.length);
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(-mCardShadowOffset, position[1], 0.01f);
    }

    // =========================================================================================
    // Get Position For Pinned Tabs
    // =========================================================================================

    @Test
    public void testGetHoverCardPosition_PinnedTab_Collapsed() {
        setContainerWidth(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ false);

        assertEquals(2, position.length);
        assertEquals(
                COLLAPSED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX + mBackgroundInset - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_PinnedTab_Expanded() {
        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ true);

        assertEquals(2, position.length);
        assertEquals(PINNED_TAB_VIEW_X_PX - mCardShadowOffset, position[0], 0.01f);
        assertEquals(
                PINNED_TAB_VIEW_Y_PX + PINNED_TAB_VIEW_HEIGHT_PX - mCardShadowOffset,
                position[1],
                0.01f);
    }

    @Test
    public void testGetHoverCardPosition_PinnedTab_Expanded_NotEnoughSpaceBelow_ShowsOnRight() {
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);
        // Window height 270:
        // relativeY + tabView.getHeight() + visibleCardHeight > parentHeight
        // (50 + 40 + 188 = 278 > 270), but top-aligned fits (50 + 188 = 238 <= 270 - 4 = 266).
        setRootViewHeight(270);
        moveView(mPinnedTabView, PINNED_TAB_VIEW_X_PX, /* y= */ 50);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ true);

        assertEquals(2, position.length);
        // Should show on the right of the rail container, top-aligned with the pinned tab.
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(50f - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void
            testGetHoverCardPosition_PinnedTab_Expanded_ExceedsRootHeight_AlignsWithWindowBottom() {
        setContainerWidth(EXPANDED_CONTAINER_WIDTH_PX);
        // Position pinned tab near bottom so that neither below-tab nor top-aligned fits:
        // relativeY + tabView.getHeight() + visibleCardHeight > parentHeight
        // (900 + 40 + 188 > 1000) and (900 + 188 > 1000 - 4 = 996).
        moveView(mPinnedTabView, PINNED_TAB_VIEW_X_PX, /* y= */ 900);

        float[] position =
                VerticalTabItemHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        inflateTabHoverCardView(),
                        /* shouldShowCardBelowTab= */ true);

        assertEquals(2, position.length);
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        // Should align with the bottom of the window leaving a hoverCardMargin:
        // hoverCardY = parentHeight - hoverCardMargin - hoverCardHeight + cardShadowOffset.
        float expectedY =
                ROOT_VIEW_HEIGHT_PX
                        - mHoverCardMarginToRail
                        - HOVER_CARD_VIEW_HEIGHT_PX
                        + mCardShadowOffset;
        assertEquals(expectedY, position[1], 0.01f);
    }

    @Test
    public void testShouldShowCardBelowTab() {
        mPinnedLayoutManager.setSpanCount(2);
        assertTrue(mController.shouldShowCardBelowTab(mPinnedTab));
        assertFalse(mController.shouldShowCardBelowTab(mTab1));

        // The pinned grid has a single column while the rail is collapsed or expanded for
        // hovering, so pinned tabs show the card next to the rail like regular tabs.
        mPinnedLayoutManager.setSpanCount(1);
        assertFalse(mController.shouldShowCardBelowTab(mPinnedTab));
        assertFalse(mController.shouldShowCardBelowTab(mTab1));
    }

    // =========================================================================================
    // Tab Group Hover Card Tests
    // =========================================================================================

    @Test
    public void testTabGroup_ShowAndHide() {
        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);
        assertEquals(View.GONE, getTabGroupHoverCardView().getVisibility());
    }

    @Test
    public void testTabGroup_EmptyGroup_DoesNotShowHoverCard() {
        Token emptyGroupId = new Token(99L, 99L);
        when(mTabModel.getTabsInGroup(emptyGroupId)).thenReturn(List.of());

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(emptyGroupId, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(getTabGroupHoverCardView().isShown());
    }

    @Test
    public void testTabGroup_NullGroupId_DoesNotShowHoverCard() {
        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                /* tabGroupId= */ null, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertFalse(getTabGroupHoverCardView().isShown());
    }

    @Test
    public void testTabGroup_DelayedShow() {
        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        assertNull(getTabGroupHoverCardView());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");
    }

    @Test
    public void testTabGroup_ExitBeforeDelay_CancelsShow() {
        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Hover exit before delay expires.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Running remaining delayed tasks should not trigger show().
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertNull(getTabGroupHoverCardView());
    }

    @Test
    public void testTabGroup_Scrubbing_ShowsImmediately() {
        TabHoverListener listener = mController.getTabHoverListener();

        // Hover group 1 and wait for delay.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");

        // Exit group 1 hover (records exit time).
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Hover group 2 within 300 ms buffer.
        listener.onTabGroupHoverStateChanged(GROUP_ID_2, mGroupHeaderView2, /* isHovered= */ true);

        // Should show group 2 immediately without needing ShadowLooper delay task flush.
        assertTabGroupHoverCardShowing("Group 2", "• Tab 1");
    }

    @Test
    public void testScrubbing_FromTabToGroup_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover tab 1 and wait for delay.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabHoverCardShowing("Tab 1");

        // Scrubbing to group 1: hover group 1 within buffer.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        // Tab hover card should be hidden, and group hover card shown immediately.
        assertEquals(View.GONE, getTabHoverCardView().getVisibility());
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");
    }

    @Test
    public void testScrubbing_FromGroupToTab_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover group 1 and wait for delay.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Scrubbing to tab 1: hover tab 1 within buffer.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Group hover card should be hidden, and tab hover card shown immediately.
        assertEquals(View.GONE, getTabGroupHoverCardView().getVisibility());
        assertTabHoverCardShowing("Tab 1");
    }

    @Test
    public void testTabGroup_Scrubbing_EnterBeforeExit_DoesNotHideGroup2() {
        TabHoverListener listener = mController.getTabHoverListener();

        // Group 1 is currently hovered and showing.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Scrubbing: Group 2 enters BEFORE Group 1 exits.
        listener.onTabGroupHoverStateChanged(GROUP_ID_2, mGroupHeaderView2, /* isHovered= */ true);
        assertTabGroupHoverCardShowing("Group 2", "• Tab 1");

        // Group 1 exits subsequently.
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Group 2 should still be showing.
        assertTabGroupHoverCardShowing("Group 2", "• Tab 1");
    }

    @Test
    public void testTabGroup_ContextMenuShowing_DoNotShow() {
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> true);

        TabHoverListener listener = controller.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertNull(getTabGroupHoverCardView());
    }

    @Test
    public void testTabGroup_KeyboardFocus_ShowsImmediately() {
        focusView(mGroupHeaderView);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");
    }

    @Test
    public void testTabGroup_HoverActiveTab_HidesGroupHoverCardImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");

        // Hover active tab (TAB_ID_1).
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Group hover card should be hidden immediately, and tab hover card should not show.
        assertEquals(View.GONE, getTabGroupHoverCardView().getVisibility());
        assertNull(getTabHoverCardView());
    }

    @Test
    public void testDestroy_CleansUpBothCards() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        listener.onTabGroupHoverStateChanged(GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // The tab hover card observes the current tab model until it is destroyed.
        assertTrue(mTabModelSupplier.hasObservers());

        mController.destroy();

        assertEquals(View.GONE, getTabHoverCardView().getVisibility());
        assertFalse(mTabModelSupplier.hasObservers());
        assertEquals(View.GONE, getTabGroupHoverCardView().getVisibility());
    }

    @Test
    public void testResetHoverState_ExecutesTagHoverStateListener() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Boolean[] lastHoverState = new Boolean[] {null};
        Callback<Boolean> hoverStateCallback = isHovered -> lastHoverState[0] = isHovered;
        mTabView1.setTag(R.id.tab_hover_state_listener, hoverStateCallback);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        mController.hideHoverCard();
        assertNull(lastHoverState[0]);

        mController.resetHoverState();
        assertEquals(false, lastHoverState[0]);
    }

    @Test
    public void testContextMenuShowing_SuppressesHoverCard() {
        boolean[] isContextMenuShowing = new boolean[] {true};
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);
        TabHoverListener listener = controller.getTabHoverListener();

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertNull(getTabHoverCardView());
        controller.destroy();
    }

    @Test
    public void testScrolling_SuppressesHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        startScrolling();

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertNull(getTabHoverCardView());
    }

    @Test
    public void testSetupTabHover_SingleHoverGuaranteeAndVisualStateUpdates() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        FrameLayout tabView2 = new FrameLayout(activity);
        tabView1.layout(0, 0, 100, 48);
        tabView2.layout(0, 48, 100, 96);

        boolean[] tab1Hovered = new boolean[] {false};
        boolean[] tab2Hovered = new boolean[] {false};
        TabHoverListener listener = mController.getTabHoverListener();

        VerticalTabItemHoverController.setupTabHover(
                listener, TAB_ID_1, tabView1, /* actionButton= */ null, v -> tab1Hovered[0] = v);
        VerticalTabItemHoverController.setupTabHover(
                listener, TAB_ID_2, tabView2, /* actionButton= */ null, v -> tab2Hovered[0] = v);

        // Hover enter on tab 1.
        MotionEvent enter1 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter1.setSource(InputDevice.SOURCE_MOUSE);
        tabView1.dispatchGenericMotionEvent(enter1);
        assertEquals(tabView1, mController.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);
        assertFalse(tab2Hovered[0]);

        // Hover enter on tab 2 without exit on tab 1 -> clears tab 1 visual state.
        MotionEvent enter2 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter2.setSource(InputDevice.SOURCE_MOUSE);
        tabView2.dispatchGenericMotionEvent(enter2);
        assertEquals(tabView2, mController.getCurrentHoveredView());
        assertFalse(tab1Hovered[0]);
        assertTrue(tab2Hovered[0]);

        // Hover exit on tab 2 outside bounds -> clears tab 2 visual state and currentHoveredView.
        MotionEvent exit2 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ -10f,
                        /* y= */ -10f,
                        /* metaState= */ 0);
        exit2.setSource(InputDevice.SOURCE_MOUSE);
        tabView2.dispatchGenericMotionEvent(exit2);
        assertNull(mController.getCurrentHoveredView());
        assertFalse(tab2Hovered[0]);
    }

    @Test
    public void testSetupTabHover_SuppressedWhenContextMenuOrScrolling() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        tabView1.layout(0, 0, 100, 48);

        boolean[] tab1Hovered = new boolean[] {false};
        boolean[] isContextMenuShowing = new boolean[] {true};
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabItemHoverController.setupTabHover(
                controller.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);

        MotionEvent enter1 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter1.setSource(InputDevice.SOURCE_MOUSE);
        tabView1.dispatchGenericMotionEvent(enter1);
        assertNull(controller.getCurrentHoveredView());
        assertFalse(tab1Hovered[0]);

        isContextMenuShowing[0] = false;
        startScrolling();
        tabView1.dispatchGenericMotionEvent(enter1);
        assertNull(controller.getCurrentHoveredView());
        assertFalse(tab1Hovered[0]);

        controller.destroy();
    }

    @Test
    public void testHoverBackgroundPersistsDuringContextMenuAndScroll_ClearedOnReset() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        FrameLayout tabView2 = new FrameLayout(activity);
        tabView1.layout(0, 0, 100, 48);
        tabView2.layout(0, 48, 100, 96);

        boolean[] tab1Hovered = new boolean[] {false};
        boolean[] tab2Hovered = new boolean[] {false};
        boolean[] isContextMenuShowing = new boolean[] {false};
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabItemHoverController.setupTabHover(
                controller.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);
        VerticalTabItemHoverController.setupTabHover(
                controller.getTabHoverListener(),
                TAB_ID_2,
                tabView2,
                /* actionButton= */ null,
                v -> tab2Hovered[0] = v);

        MotionEvent enter =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter.setSource(InputDevice.SOURCE_MOUSE);
        MotionEvent exit =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ -10f,
                        /* y= */ -10f,
                        /* metaState= */ 0);
        exit.setSource(InputDevice.SOURCE_MOUSE);

        // 1. Hover tab 1, then open context menu and hide hover card.
        tabView1.dispatchGenericMotionEvent(enter);
        assertEquals(tabView1, controller.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);

        isContextMenuShowing[0] = true;
        controller.hideHoverCard();

        // Exiting tab 1 or entering tab 2 while context menu is open keeps tab 1 hovered.
        tabView1.dispatchGenericMotionEvent(exit);
        tabView2.dispatchGenericMotionEvent(enter);
        assertEquals(tabView1, controller.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);
        assertFalse(tab2Hovered[0]);

        // Dismissing context menu and resetting hover state clears tab 1 hover background.
        isContextMenuShowing[0] = false;
        controller.resetHoverState();
        assertNull(controller.getCurrentHoveredView());
        assertFalse(tab1Hovered[0]);

        // 2. Hover tab 1, then start scrolling and hide hover card.
        tabView1.dispatchGenericMotionEvent(enter);
        assertEquals(tabView1, controller.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);

        startScrolling();
        controller.hideHoverCard();

        // Exiting tab 1 while scrolling keeps tab 1 hovered.
        tabView1.dispatchGenericMotionEvent(exit);
        assertEquals(tabView1, controller.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);

        // Scroll returning to idle and resetting hover state clears tab 1 hover background.
        mRecyclerView.stopScroll();
        controller.resetHoverState();
        assertNull(controller.getCurrentHoveredView());
        assertFalse(tab1Hovered[0]);

        controller.destroy();
    }

    @Test
    public void testSetupTabGroupHeaderHover_SingleHoverGuaranteeAndVisualStateUpdates() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout groupView1 = new FrameLayout(activity);
        View menuButton = new View(activity);
        menuButton.setId(R.id.menu_button);
        groupView1.addView(menuButton);
        FrameLayout tabView1 = new FrameLayout(activity);
        groupView1.layout(0, 0, 100, 48);
        tabView1.layout(0, 48, 100, 96);

        boolean[] group1Hovered = new boolean[] {false};
        boolean[] tab1Hovered = new boolean[] {false};
        TabHoverListener listener = mController.getTabHoverListener();

        VerticalTabItemHoverController.setupTabGroupHeaderHover(
                listener, GROUP_ID_1, groupView1, menuButton, v -> group1Hovered[0] = v);
        VerticalTabItemHoverController.setupTabHover(
                listener, TAB_ID_1, tabView1, /* actionButton= */ null, v -> tab1Hovered[0] = v);

        UserActionTester userActionTester = new UserActionTester();

        // Hover enter on group header 1 while rail is expanded -> records GroupHeaderHovered.
        mContainerView.setCollapseState(RailCollapseState.EXPANDED);
        MotionEvent enter1 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter1.setSource(InputDevice.SOURCE_MOUSE);
        groupView1.dispatchGenericMotionEvent(enter1);
        assertEquals(groupView1, mController.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);
        assertFalse(tab1Hovered[0]);
        assertEquals(1, userActionTester.getActionCount("Android.VerticalTabs.GroupHeaderHovered"));

        // Hover enter on tab 1 without exit on group header 1 -> clears group header 1 visual
        // state.
        MotionEvent enter2 =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter2.setSource(InputDevice.SOURCE_MOUSE);
        tabView1.dispatchGenericMotionEvent(enter2);
        assertEquals(tabView1, mController.getCurrentHoveredView());
        assertFalse(group1Hovered[0]);
        assertTrue(tab1Hovered[0]);

        // Hover enter back on group header 1 while rail is collapsed -> clears tab 1 visual state,
        // does not record GroupHeaderHovered.
        mContainerView.setCollapseState(RailCollapseState.COLLAPSED);
        groupView1.dispatchGenericMotionEvent(enter1);
        assertEquals(groupView1, mController.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);
        assertFalse(tab1Hovered[0]);
        assertEquals(1, userActionTester.getActionCount("Android.VerticalTabs.GroupHeaderHovered"));
    }

    @Test
    public void testTabGroupHeaderHoverStatePersistsDuringContextMenuAndScroll_ClearedOnReset() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout groupView1 = new FrameLayout(activity);
        groupView1.layout(0, 0, 100, 40);
        FrameLayout groupView2 = new FrameLayout(activity);
        groupView2.layout(0, 40, 100, 80);

        boolean[] group1Hovered = new boolean[] {false};
        boolean[] group2Hovered = new boolean[] {false};
        boolean[] isContextMenuShowing = new boolean[] {false};
        VerticalTabItemHoverController controller =
                new VerticalTabItemHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabItemHoverController.setupTabGroupHeaderHover(
                controller.getTabHoverListener(),
                GROUP_ID_1,
                groupView1,
                /* menuButton= */ null,
                v -> group1Hovered[0] = v);
        VerticalTabItemHoverController.setupTabGroupHeaderHover(
                controller.getTabHoverListener(),
                GROUP_ID_2,
                groupView2,
                /* menuButton= */ null,
                v -> group2Hovered[0] = v);

        MotionEvent enter =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter.setSource(InputDevice.SOURCE_MOUSE);
        MotionEvent exit =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ -10f,
                        /* y= */ -10f,
                        /* metaState= */ 0);
        exit.setSource(InputDevice.SOURCE_MOUSE);

        // 1. Hover group header 1, then open context menu and hide hover card.
        groupView1.dispatchGenericMotionEvent(enter);
        assertEquals(groupView1, controller.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);

        isContextMenuShowing[0] = true;
        controller.hideHoverCard();

        // Exiting group 1 or entering group 2 while context menu is open keeps group 1 hovered.
        groupView1.dispatchGenericMotionEvent(exit);
        groupView2.dispatchGenericMotionEvent(enter);
        assertEquals(groupView1, controller.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);
        assertFalse(group2Hovered[0]);

        // Dismissing context menu and resetting hover state clears group 1 hover state.
        isContextMenuShowing[0] = false;
        controller.resetHoverState();
        assertNull(controller.getCurrentHoveredView());
        assertFalse(group1Hovered[0]);

        // 2. Hover group header 1, then start scrolling and hide hover card.
        groupView1.dispatchGenericMotionEvent(enter);
        assertEquals(groupView1, controller.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);

        startScrolling();
        controller.hideHoverCard();

        // Exiting group 1 or entering group 2 while scrolling keeps group 1 hovered.
        groupView1.dispatchGenericMotionEvent(exit);
        groupView2.dispatchGenericMotionEvent(enter);
        assertEquals(groupView1, controller.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);
        assertFalse(group2Hovered[0]);

        // Scroll returning to idle and resetting hover state clears group 1 hover state.
        mRecyclerView.stopScroll();
        controller.resetHoverState();
        assertNull(controller.getCurrentHoveredView());
        assertFalse(group1Hovered[0]);

        controller.destroy();
    }

    @Test
    public void testSetupTabHover_FocusChange_RoutesToListener() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 =
                new FrameLayout(activity) {
                    @Override
                    public boolean hasFocus() {
                        return true;
                    }
                };
        tabView1.layout(0, 0, 100, 48);

        VerticalTabItemHoverController.setupTabHover(
                mController.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> {});

        assertNotNull(tabView1.getOnFocusChangeListener());
        tabView1.getOnFocusChangeListener().onFocusChange(tabView1, /* hasFocus= */ true);

        assertEquals(tabView1, mController.getCurrentHoveredView());
        assertTabHoverCardShowing("Tab 1");

        tabView1.getOnFocusChangeListener().onFocusChange(tabView1, /* hasFocus= */ false);
        assertNull(mController.getCurrentHoveredView());
        assertEquals(View.GONE, getTabHoverCardView().getVisibility());
    }

    @Test
    public void testSetupTabGroupHeaderHover_FocusChange_RoutesToListener() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout groupView1 =
                new FrameLayout(activity) {
                    @Override
                    public boolean hasFocus() {
                        return true;
                    }
                };
        groupView1.layout(0, 0, 100, 48);

        VerticalTabItemHoverController.setupTabGroupHeaderHover(
                mController.getTabHoverListener(),
                GROUP_ID_1,
                groupView1,
                /* menuButton= */ null,
                v -> {});

        assertNotNull(groupView1.getOnFocusChangeListener());
        groupView1.getOnFocusChangeListener().onFocusChange(groupView1, /* hasFocus= */ true);

        assertEquals(groupView1, mController.getCurrentHoveredView());
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");

        groupView1.getOnFocusChangeListener().onFocusChange(groupView1, /* hasFocus= */ false);
        assertNull(mController.getCurrentHoveredView());
        assertEquals(View.GONE, getTabGroupHoverCardView().getVisibility());
    }

    @Test
    public void testSetupItemHover_NullListener_UpdatesVisualStateDirectly() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        tabView1.layout(0, 0, 100, 48);

        boolean[] tab1Hovered = new boolean[] {false};
        VerticalTabItemHoverController.setupTabHover(
                /* listener= */ null,
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);

        MotionEvent enter =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter.setSource(InputDevice.SOURCE_MOUSE);
        tabView1.dispatchGenericMotionEvent(enter);
        assertTrue(tabView1.isHovered());
        assertTrue(tab1Hovered[0]);

        MotionEvent exit =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ -10f,
                        /* y= */ -10f,
                        /* metaState= */ 0);
        exit.setSource(InputDevice.SOURCE_MOUSE);
        tabView1.dispatchGenericMotionEvent(exit);
        assertFalse(tabView1.isHovered());
        assertFalse(tab1Hovered[0]);

        // Focus change with null listener should safely no-op.
        tabView1.getOnFocusChangeListener().onFocusChange(tabView1, /* hasFocus= */ true);
        assertFalse(tab1Hovered[0]);
    }

    @Test
    public void testSetupItemHover_ChildButtonHoverOrchestration() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        View actionButton = new View(activity);
        tabView1.addView(actionButton);
        tabView1.layout(0, 0, 100, 48);
        actionButton.layout(70, 8, 94, 32);

        boolean[] tab1Hovered = new boolean[] {false};
        VerticalTabItemHoverController.setupTabHover(
                mController.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                actionButton,
                v -> tab1Hovered[0] = v);

        // 1. Hover enter and move on child button -> sets child and parent hovered.
        MotionEvent childEnter =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 5f,
                        /* y= */ 5f,
                        /* metaState= */ 0);
        childEnter.setSource(InputDevice.SOURCE_MOUSE);
        actionButton.dispatchGenericMotionEvent(childEnter);
        assertTrue(actionButton.isHovered());
        assertTrue(tabView1.isHovered());
        assertTrue(tab1Hovered[0]);
        assertEquals(tabView1, mController.getCurrentHoveredView());

        MotionEvent childMove =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_MOVE,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        childMove.setSource(InputDevice.SOURCE_MOUSE);
        actionButton.dispatchGenericMotionEvent(childMove);
        assertTrue(actionButton.isHovered());
        assertTrue(tabView1.isHovered());
        assertTrue(tab1Hovered[0]);

        // 2. Hover exit from child button while staying inside parent bounds (left = 70 + (-10) =
        // 60, top = 8 + 10 = 18) -> clears child hover but preserves parent hover state.
        MotionEvent childExitInsideParent =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ -10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        childExitInsideParent.setSource(InputDevice.SOURCE_MOUSE);
        actionButton.dispatchGenericMotionEvent(childExitInsideParent);
        assertFalse(actionButton.isHovered());
        assertTrue(tabView1.isHovered());
        assertTrue(tab1Hovered[0]);
        assertEquals(tabView1, mController.getCurrentHoveredView());

        // 3. Re-enter child button, then hover exit outside parent bounds (left = 70 + 40 = 110 >=
        // 100) -> clears both child and parent hover state.
        actionButton.dispatchGenericMotionEvent(childEnter);
        assertTrue(actionButton.isHovered());

        MotionEvent childExitOutsideParent =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_EXIT,
                        /* x= */ 40f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        childExitOutsideParent.setSource(InputDevice.SOURCE_MOUSE);
        actionButton.dispatchGenericMotionEvent(childExitOutsideParent);
        assertFalse(actionButton.isHovered());
        assertFalse(tabView1.isHovered());
        assertFalse(tab1Hovered[0]);
        assertNull(mController.getCurrentHoveredView());
    }

    @Test
    public void testSetupItemHover_AlreadyHoveredView_AppliesVisualStateImmediately() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        tabView1.setHovered(true);

        boolean[] tab1Hovered = new boolean[] {false};
        VerticalTabItemHoverController.setupTabHover(
                mController.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);

        assertTrue(tab1Hovered[0]);
    }

    @Test
    public void testSetupTabGroupHeaderHover_RoutesToGroupHoverListener() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout groupView1 = new FrameLayout(activity);
        groupView1.layout(0, 0, 100, 48);

        boolean[] group1Hovered = new boolean[] {false};
        VerticalTabItemHoverController.setupTabGroupHeaderHover(
                mController.getTabHoverListener(),
                GROUP_ID_1,
                groupView1,
                /* menuButton= */ null,
                v -> group1Hovered[0] = v);

        MotionEvent enter =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        enter.setSource(InputDevice.SOURCE_MOUSE);
        groupView1.dispatchGenericMotionEvent(enter);

        assertTrue(groupView1.isHovered());
        assertTrue(group1Hovered[0]);
        assertEquals(groupView1, mController.getCurrentHoveredView());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTabGroupHoverCardShowing("Group 1", "• Tab 1", "• Tab 2");
    }

    /** Adds an item view at the given position in the root view. */
    private View addItemView(int x, int y, int height) {
        View view = new View(mRootView.getContext());
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT, height);
        params.leftMargin = x;
        params.topMargin = y;
        mRootView.addView(view, params);
        return view;
    }

    /** Moves the given child of the root view, and runs a layout pass. */
    private static void moveView(View view, int x, int y) {
        LayoutParams params = (LayoutParams) view.getLayoutParams();
        params.leftMargin = x;
        params.topMargin = y;
        view.setLayoutParams(params);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void setContainerWidth(int widthPx) {
        ViewGroup.LayoutParams params = mContainerView.getLayoutParams();
        params.width = widthPx;
        mContainerView.setLayoutParams(params);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void setRootViewHeight(int heightPx) {
        ViewGroup.LayoutParams params = mRootView.getLayoutParams();
        params.height = heightPx;
        mRootView.setLayoutParams(params);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private static void focusView(View view) {
        view.setFocusableInTouchMode(true);
        assertTrue(view.requestFocus());
    }

    /** Moves the scroll state of the tab list away from {@link RecyclerView#SCROLL_STATE_IDLE}. */
    private void startScrolling() {
        mRecyclerView.smoothScrollBy(/* dx= */ 0, /* dy= */ 100);
        assertEquals(RecyclerView.SCROLL_STATE_SETTLING, mRecyclerView.getScrollState());
    }

    /** Inflates the tab hover card with a fixed height to test the positioning logic. */
    private TabHoverCardView inflateTabHoverCardView() {
        TabHoverCardView tabHoverCardView = (TabHoverCardView) mTabHoverCardViewStub.inflate();
        tabHoverCardView.setMinimumHeight(HOVER_CARD_VIEW_HEIGHT_PX);
        return tabHoverCardView;
    }

    private TabHoverCardView getTabHoverCardView() {
        return mRootView.findViewById(R.id.tab_hover_card_holder);
    }

    private TabGroupHoverCardView getTabGroupHoverCardView() {
        return mRootView.findViewById(R.id.vertical_tab_group_hover_card_holder);
    }

    private void assertTabHoverCardShowing(String tabTitle) {
        TabHoverCardView tabHoverCardView = getTabHoverCardView();
        assertNotNull(tabHoverCardView);
        assertTrue(tabHoverCardView.isShown());
        TextView titleView = tabHoverCardView.findViewById(R.id.title);
        assertEquals(tabTitle, titleView.getText().toString());
    }

    private void assertTabGroupHoverCardShowing(String groupTitle, String... tabTitles) {
        TabGroupHoverCardView tabGroupHoverCardView = getTabGroupHoverCardView();
        assertNotNull(tabGroupHoverCardView);
        assertTrue(tabGroupHoverCardView.isShown());
        TextView groupTitleView = tabGroupHoverCardView.findViewById(R.id.group_title);
        assertEquals(groupTitle, groupTitleView.getText().toString());

        List<String> shownTabTitles = new ArrayList<>();
        for (int id :
                new int[] {
                    R.id.group_child_tab_1,
                    R.id.group_child_tab_2,
                    R.id.group_child_tab_3,
                    R.id.group_child_tab_4,
                    R.id.group_child_tab_5
                }) {
            TextView childTabView = tabGroupHoverCardView.findViewById(id);
            if (childTabView.getVisibility() == View.VISIBLE) {
                shownTabTitles.add(childTabView.getText().toString());
            }
        }
        assertEquals(List.of(tabTitles), shownTabTitles);
    }
}
