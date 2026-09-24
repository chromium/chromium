// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverController.TabHoverListener;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;

import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabHoverController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabHoverControllerUnitTest {

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int PINNED_TAB_ID = 4;
    private static final int GROUP_HEADER_TAB_ID_1 = 10;
    private static final int GROUP_HEADER_TAB_ID_2 = 20;
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

    @Mock private VerticalTabRailLayout mContainerView;
    @Mock private VerticalTabListRecyclerView mRecyclerView;
    @Mock private ViewGroup mRootView;
    @Mock private View mTabView1;
    @Mock private View mTabView2;
    @Mock private View mPinnedTabView;
    @Mock private View mGroupHeaderView;
    @Mock private View mGroupHeaderView2;
    @Mock private ViewGroup mViewStubParent;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private ViewStub mTabGroupHoverCardViewStub;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private TabGroupHoverCardView mTabGroupHoverCardView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mPinnedTab;

    private VerticalTabHoverController mController;

    private float mCardShadowOffset;
    private float mBackgroundInset;
    private float mHoverCardMarginToRail;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mCardShadowOffset = activity.getResources().getDimension(R.dimen.popup_menu_shadow_length);
        mBackgroundInset =
                activity.getResources().getDimension(R.dimen.vertical_tab_item_touch_target_inset);
        mHoverCardMarginToRail =
                activity.getResources()
                        .getDimension(R.dimen.vertical_tab_hover_card_margin_to_rail);

        when(mContainerView.getContext()).thenReturn(activity);
        when(mContainerView.getRecyclerView()).thenReturn(mRecyclerView);
        when(mContainerView.getRootView()).thenReturn(mRootView);
        when(mRootView.getHeight()).thenReturn(ROOT_VIEW_HEIGHT_PX);
        when(mTabHoverCardView.getContext()).thenReturn(activity);
        when(mTabHoverCardView.getParent()).thenReturn(mRootView);
        when(mTabHoverCardView.getMeasuredHeight()).thenReturn(HOVER_CARD_VIEW_HEIGHT_PX);
        when(mTabGroupHoverCardView.getContext()).thenReturn(activity);
        when(mTabGroupHoverCardView.getMeasuredHeight()).thenReturn(HOVER_CARD_VIEW_HEIGHT_PX);
        when(mTabHoverCardViewStub.getParent()).thenReturn(mViewStubParent);
        when(mTabGroupHoverCardViewStub.getParent()).thenReturn(mViewStubParent);
        when(mTabView1.getHeight()).thenReturn(REGULAR_TAB_VIEW_HEIGHT_PX);
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = 0;
                            array[1] = TAB_VIEW_Y_PX;
                            return null;
                        })
                .when(mTabView1)
                .getLocationOnScreen(any());

        when(mPinnedTabView.getHeight()).thenReturn(PINNED_TAB_VIEW_HEIGHT_PX);
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = PINNED_TAB_VIEW_X_PX;
                            array[1] = PINNED_TAB_VIEW_Y_PX;
                            return null;
                        })
                .when(mPinnedTabView)
                .getLocationOnScreen(any());

        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            doAnswer(
                                            inflateInvocation -> {
                                                if (listener != null) {
                                                    listener.onInflate(
                                                            mTabHoverCardViewStub,
                                                            mTabHoverCardView);
                                                }
                                                return mTabHoverCardView;
                                            })
                                    .when(mTabHoverCardViewStub)
                                    .inflate();
                            return null;
                        })
                .when(mTabHoverCardViewStub)
                .setOnInflateListener(any());

        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            doAnswer(
                                            inflateInvocation -> {
                                                if (listener != null) {
                                                    listener.onInflate(
                                                            mTabGroupHoverCardViewStub,
                                                            mTabGroupHoverCardView);
                                                }
                                                return mTabGroupHoverCardView;
                                            })
                                    .when(mTabGroupHoverCardViewStub)
                                    .inflate();
                            return null;
                        })
                .when(mTabGroupHoverCardViewStub)
                .setOnInflateListener(any());

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mPinnedTab.getId()).thenReturn(PINNED_TAB_ID);
        when(mPinnedTab.getIsPinned()).thenReturn(true);
        when(mTab1.getTitle()).thenReturn("Tab 1");
        when(mTab2.getTitle()).thenReturn("Tab 2");
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModelSelector.getTabById(PINNED_TAB_ID)).thenReturn(mPinnedTab);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getTabsInGroup(GROUP_ID_1)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.getTabsInGroup(GROUP_ID_2)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(GROUP_ID_1)).thenReturn("Group 1");
        when(mTabModel.getTabGroupTitle(GROUP_ID_2)).thenReturn("Group 2");

        mController =
                new VerticalTabHoverController(
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

        verify(mTabHoverCardViewStub).inflate();

        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        inOrder.verify(mTabHoverCardView).hide();
    }

    @Test
    public void testSelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    @Test
    public void testContextMenuShowing_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
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

        verify(mTabHoverCardViewStub, never()).inflate();
    }

    // =========================================================================================
    // Delay
    // =========================================================================================

    @Test
    public void testGetHoverCardDelay() {
        // Collapsed rail width (76dp) -> 300ms
        when(mContainerView.getWidth())
                .thenReturn(VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP);
        assertEquals(TabHoverCardView.MIN_HOVER_CARD_DELAY_MS, mController.getHoverCardDelay());

        // Expanded rail width (240dp) -> 800ms
        when(mContainerView.getWidth()).thenReturn(VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP);
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
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
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
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    // =========================================================================================
    // Scrubbing Between Tabs
    // =========================================================================================

    @Test
    public void testScrubbing_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover tab 1 and wait for delay
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        clearInvocations(mTabHoverCardView);

        // Exit tab 1 hover (records exit time)
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Hover tab 2 within 300 ms buffer
        listener.onTabHoverStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);

        // Should show tab 2 immediately without needing ShadowLooper delay task flush
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testScrubbing_EnterBeforeExit_HidesBeforeShowingTab2() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Tab 1 is currently hovered and showing.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Scrubbing: Tab 2 enters BEFORE Tab 1 exits (due to ViewGroup dispatch order).
        listener.onTabHoverStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab2));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Clear previous invocations to accurately verify the exit behavior.
        clearInvocations(mTabHoverCardView);

        // Tab 1 exits subsequently.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Tab 2 should still be showing and hide() should NOT be called again.
        verify(mTabHoverCardView, never()).hide();
    }

    // =========================================================================================
    // Keyboard Focus
    // =========================================================================================

    @Test
    public void testKeyboardFocus_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Should show immediately for keyboard focus without needing ShadowLooper delay task flush.
        verify(mTabHoverCardViewStub).inflate();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testKeyboardFocus_FocusLost_HidesHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Inflation initializes and hides the view before showing.
        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Focus lost triggers hide.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        inOrder.verify(mTabHoverCardView).hide();
    }

    @Test
    public void testKeyboardFocus_SelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    // =========================================================================================
    // Repositions
    // =========================================================================================

    @Test
    public void testCardHeightChange_RepositionsHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        ArgumentCaptor<Runnable> callbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mTabHoverCardView).setOnCardHeightChangedCallback(callbackCaptor.capture());
        Runnable heightChangedCallback = callbackCaptor.getValue();
        assertNotNull(heightChangedCallback);

        // When height changes (e.g. from 200 to 250 due to memory usage visibility):
        when(mTabHoverCardView.getMeasuredHeight()).thenReturn(250);
        heightChangedCallback.run();

        verify(mTabHoverCardView).setX(anyFloat());
        verify(mTabHoverCardView).setY(anyFloat());
    }

    // =========================================================================================
    // Get Position For Regular Tabs
    // =========================================================================================

    @Test
    public void testGetHoverCardPosition_RegularTab_Collapsed() {
        when(mContainerView.getWidth()).thenReturn(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ false,
                        /* isRailCollapsed= */ true);

        assertEquals(2, position.length);
        assertEquals(
                COLLAPSED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_RegularTab_Expanded() {
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ false,
                        /* isRailCollapsed= */ false);

        assertEquals(2, position.length);
        assertEquals(
                EXPANDED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX + mBackgroundInset - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_ExceedsRootHeight_AlignsWithWindowBottom() {
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);
        // Position the tab view near the bottom of the window so that the hover card extends
        // beyond the root view height (900 + 188 > 1000 - 4).
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = 0;
                            array[1] = 900;
                            return null;
                        })
                .when(mTabView1)
                .getLocationOnScreen(any());

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ false,
                        /* isRailCollapsed= */ false);

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
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);
        // Window height 180, tab at Y = 50:
        // Window bottom alignment shifts visible card top above 0 (180 - 4 - 188 = -12).
        // Clamps visibleY to 0, so hoverCardY = 0 - cardShadowOffset = -cardShadowOffset.
        when(mRootView.getHeight()).thenReturn(180);
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = 0;
                            array[1] = 50;
                            return null;
                        })
                .when(mTabView1)
                .getLocationOnScreen(any());

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ false,
                        /* isRailCollapsed= */ false);

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
        when(mContainerView.getWidth()).thenReturn(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ true,
                        /* isRailCollapsed= */ true);

        assertEquals(2, position.length);
        assertEquals(
                COLLAPSED_CONTAINER_WIDTH_PX - mCardShadowOffset + mHoverCardMarginToRail,
                position[0],
                0.01f);
        assertEquals(TAB_VIEW_Y_PX - mCardShadowOffset, position[1], 0.01f);
    }

    @Test
    public void testGetHoverCardPosition_PinnedTab_Expanded() {
        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ true,
                        /* isRailCollapsed= */ false);

        assertEquals(2, position.length);
        assertEquals(PINNED_TAB_VIEW_X_PX - mCardShadowOffset, position[0], 0.01f);
        assertEquals(
                PINNED_TAB_VIEW_Y_PX + PINNED_TAB_VIEW_HEIGHT_PX - mCardShadowOffset,
                position[1],
                0.01f);
    }

    @Test
    public void testGetHoverCardPosition_PinnedTab_Expanded_NotEnoughSpaceBelow_ShowsOnRight() {
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);
        // Window height 270:
        // relativeY + tabView.getHeight() + visibleCardHeight > parentHeight
        // (50 + 40 + 188 = 278 > 270), but top-aligned fits (50 + 188 = 238 <= 270 - 4 = 266).
        when(mRootView.getHeight()).thenReturn(270);
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = PINNED_TAB_VIEW_X_PX;
                            array[1] = 50;
                            return null;
                        })
                .when(mPinnedTabView)
                .getLocationOnScreen(any());

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ true,
                        /* isRailCollapsed= */ false);

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
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);
        // Position pinned tab near bottom so that neither below-tab nor top-aligned fits:
        // relativeY + tabView.getHeight() + visibleCardHeight > parentHeight
        // (900 + 40 + 188 > 1000) and (900 + 188 > 1000 - 4 = 996).
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[0] = PINNED_TAB_VIEW_X_PX;
                            array[1] = 900;
                            return null;
                        })
                .when(mPinnedTabView)
                .getLocationOnScreen(any());

        float[] position =
                VerticalTabHoverController.getHoverCardPosition(
                        mPinnedTabView,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ true,
                        /* isRailCollapsed= */ false);

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

    // =========================================================================================
    // Tab Group Hover Card Tests
    // =========================================================================================

    @Test
    public void testTabGroup_ShowAndHide() {
        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabGroupHoverCardViewStub).inflate();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);
        verify(mTabGroupHoverCardView).hide();
    }

    @Test
    public void testTabGroup_EmptyGroup_DoesNotShowHoverCard() {
        Token emptyGroupId = new Token(99L, 99L);
        when(mTabModel.getTabsInGroup(emptyGroupId)).thenReturn(List.of());

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, emptyGroupId, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabGroupHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_NullGroupId_ResolvesAndShows() {
        when(mTabModel.getTabById(GROUP_HEADER_TAB_ID_1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(GROUP_ID_1);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1,
                /* tabGroupId= */ null,
                mGroupHeaderView,
                /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabGroupHoverCardViewStub).inflate();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1,
                /* tabGroupId= */ null,
                mGroupHeaderView,
                /* isHovered= */ false);
        verify(mTabGroupHoverCardView).hide();
    }

    @Test
    public void testTabGroup_DelayedShow() {
        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        verify(mTabGroupHoverCardView, never()).show(anyFloat(), anyFloat());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_ExitBeforeDelay_CancelsShow() {
        TabHoverListener listener = mController.getTabHoverListener();

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Hover exit before delay expires.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Running remaining delayed tasks should not trigger show().
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabGroupHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_Scrubbing_ShowsImmediately() {
        when(mTabGroupHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover group 1 and wait for delay.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        // Exit group 1 hover (records exit time).
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Hover group 2 within 300 ms buffer.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_2, GROUP_ID_2, mGroupHeaderView2, /* isHovered= */ true);

        // Should show group 2 immediately without needing ShadowLooper delay task flush.
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 2"), eq(List.of("• Tab 1")), eq(0), eq(false));
        verify(mTabGroupHoverCardView, times(2)).show(anyFloat(), anyFloat());
    }

    @Test
    public void testScrubbing_FromTabToGroup_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover tab 1 and wait for delay.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).bindTab(eq(mTab1));
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Scrubbing to group 1: hover group 1 within buffer.
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        // Tab hover card should be hidden, and group hover card shown immediately.
        verify(mTabHoverCardView, atLeastOnce()).hide();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testScrubbing_FromGroupToTab_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabGroupHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Hover group 1 and wait for delay.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Scrubbing to tab 1: hover tab 1 within buffer.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Group hover card should be hidden, and tab hover card shown immediately.
        verify(mTabGroupHoverCardView, atLeastOnce()).hide();
        verify(mTabHoverCardView).bindTab(eq(mTab1));
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_Scrubbing_EnterBeforeExit_DoesNotHideGroup2() {
        when(mTabGroupHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();

        // Group 1 is currently hovered and showing.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Scrubbing: Group 2 enters BEFORE Group 1 exits.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_2, GROUP_ID_2, mGroupHeaderView2, /* isHovered= */ true);
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 2"), eq(List.of("• Tab 1")), eq(0), eq(false));
        verify(mTabGroupHoverCardView, times(2)).show(anyFloat(), anyFloat());

        // Clear previous invocations to accurately verify the exit behavior.
        clearInvocations(mTabGroupHoverCardView);

        // Group 1 exits subsequently.
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        // Group 2 should still be showing and hide() should NOT be called.
        verify(mTabGroupHoverCardView, never()).hide();
    }

    @Test
    public void testTabGroup_ContextMenuShowing_DoNotShow() {
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> true);

        TabHoverListener listener = controller.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabGroupHoverCardViewStub, never()).inflate();
    }

    @Test
    public void testTabGroup_KeyboardFocus_ShowsImmediately() {
        when(mGroupHeaderView.hasFocus()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        assertNotNull(listener);

        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);

        verify(mTabGroupHoverCardViewStub).inflate();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_HoverActiveTab_HidesGroupHoverCardImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);
        when(mTabGroupHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        // Hover active tab (TAB_ID_1).
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Group hover card should be hidden immediately, and tab hover card should not show.
        verify(mTabGroupHoverCardView, atLeastOnce()).hide();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    @Test
    public void testTabGroup_ExitWithOnlyGroupId_HidesCard() {
        when(mTabGroupHoverCardView.isShown()).thenReturn(true);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        // Exit with invalid header tab ID but matching Token.
        listener.onTabGroupHoverStateChanged(
                Tab.INVALID_TAB_ID, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ false);

        verify(mTabGroupHoverCardView, atLeastOnce()).hide();
    }

    @Test
    public void testDestroy_CleansUpBothCards() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        listener.onTabGroupHoverStateChanged(
                GROUP_HEADER_TAB_ID_1, GROUP_ID_1, mGroupHeaderView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mController.destroy();

        verify(mTabHoverCardView, atLeastOnce()).hide();
        verify(mTabHoverCardView).destroy();
        verify(mTabGroupHoverCardView, atLeastOnce()).hide();
        verify(mTabGroupHoverCardView).destroy();
    }

    @Test
    public void testResetHoverState_ExecutesTagHoverStateListener() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        Boolean[] lastHoverState = new Boolean[] {null};
        Callback<Boolean> hoverStateCallback = isHovered -> lastHoverState[0] = isHovered;
        when(mTabView1.getTag(R.id.tab_hover_state_listener)).thenReturn(hoverStateCallback);

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
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);
        TabHoverListener listener = controller.getTabHoverListener();

        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
        controller.destroy();
    }

    @Test
    public void testScrolling_SuppressesHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mRecyclerView.getScrollState())
                .thenReturn(androidx.recyclerview.widget.RecyclerView.SCROLL_STATE_DRAGGING);

        TabHoverListener listener = mController.getTabHoverListener();
        listener.onTabHoverStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
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

        VerticalTabHoverController.setupTabHover(
                listener, TAB_ID_1, tabView1, /* actionButton= */ null, v -> tab1Hovered[0] = v);
        VerticalTabHoverController.setupTabHover(
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
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabHoverController.setupTabHover(
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
        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_DRAGGING);
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
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabHoverController.setupTabHover(
                controller.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);
        VerticalTabHoverController.setupTabHover(
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

        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_DRAGGING);
        controller.hideHoverCard();

        // Exiting tab 1 while scrolling keeps tab 1 hovered.
        tabView1.dispatchGenericMotionEvent(exit);
        assertEquals(tabView1, controller.getCurrentHoveredView());
        assertTrue(tab1Hovered[0]);

        // Scroll returning to idle and resetting hover state clears tab 1 hover background.
        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);
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

        VerticalTabHoverController.setupTabGroupHeaderHover(
                listener,
                GROUP_HEADER_TAB_ID_1,
                GROUP_ID_1,
                groupView1,
                menuButton,
                v -> group1Hovered[0] = v);
        VerticalTabHoverController.setupTabHover(
                listener, TAB_ID_1, tabView1, /* actionButton= */ null, v -> tab1Hovered[0] = v);

        UserActionTester userActionTester = new UserActionTester();

        // Hover enter on group header 1 while rail is expanded -> records GroupHeaderHovered.
        when(mContainerView.isCollapsed()).thenReturn(false);
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
        when(mContainerView.isCollapsed()).thenReturn(true);
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
        VerticalTabHoverController controller =
                new VerticalTabHoverController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> isContextMenuShowing[0]);

        VerticalTabHoverController.setupTabGroupHeaderHover(
                controller.getTabHoverListener(),
                GROUP_HEADER_TAB_ID_1,
                GROUP_ID_1,
                groupView1,
                /* menuButton= */ null,
                v -> group1Hovered[0] = v);
        VerticalTabHoverController.setupTabGroupHeaderHover(
                controller.getTabHoverListener(),
                GROUP_HEADER_TAB_ID_2,
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

        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_DRAGGING);
        controller.hideHoverCard();

        // Exiting group 1 or entering group 2 while scrolling keeps group 1 hovered.
        groupView1.dispatchGenericMotionEvent(exit);
        groupView2.dispatchGenericMotionEvent(enter);
        assertEquals(groupView1, controller.getCurrentHoveredView());
        assertTrue(group1Hovered[0]);
        assertFalse(group2Hovered[0]);

        // Scroll returning to idle and resetting hover state clears group 1 hover state.
        when(mRecyclerView.getScrollState()).thenReturn(RecyclerView.SCROLL_STATE_IDLE);
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

        VerticalTabHoverController.setupTabHover(
                mController.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> {});

        assertNotNull(tabView1.getOnFocusChangeListener());
        tabView1.getOnFocusChangeListener().onFocusChange(tabView1, /* hasFocus= */ true);

        assertEquals(tabView1, mController.getCurrentHoveredView());
        verify(mTabHoverCardViewStub).inflate();
        verify(mTabHoverCardView).bindTab(eq(mTab1));
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        tabView1.getOnFocusChangeListener().onFocusChange(tabView1, /* hasFocus= */ false);
        assertNull(mController.getCurrentHoveredView());
        verify(mTabHoverCardView, times(2)).hide();
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

        VerticalTabHoverController.setupTabGroupHeaderHover(
                mController.getTabHoverListener(),
                GROUP_HEADER_TAB_ID_1,
                GROUP_ID_1,
                groupView1,
                /* menuButton= */ null,
                v -> {});

        assertNotNull(groupView1.getOnFocusChangeListener());
        groupView1.getOnFocusChangeListener().onFocusChange(groupView1, /* hasFocus= */ true);

        assertEquals(groupView1, mController.getCurrentHoveredView());
        verify(mTabGroupHoverCardViewStub).inflate();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());

        groupView1.getOnFocusChangeListener().onFocusChange(groupView1, /* hasFocus= */ false);
        assertNull(mController.getCurrentHoveredView());
        verify(mTabGroupHoverCardView).hide();
    }

    @Test
    public void testSetupItemHover_NullListener_UpdatesVisualStateDirectly() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout tabView1 = new FrameLayout(activity);
        tabView1.layout(0, 0, 100, 48);

        boolean[] tab1Hovered = new boolean[] {false};
        VerticalTabHoverController.setupTabHover(
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
        VerticalTabHoverController.setupTabHover(
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
        VerticalTabHoverController.setupTabHover(
                mController.getTabHoverListener(),
                TAB_ID_1,
                tabView1,
                /* actionButton= */ null,
                v -> tab1Hovered[0] = v);

        assertTrue(tab1Hovered[0]);
    }

    @Test
    public void testSetupTabGroupHeaderHover_NullTabGroupId_RoutesToGroupHoverListener() {
        when(mTabModel.getTabById(GROUP_HEADER_TAB_ID_1)).thenReturn(mTab1);
        when(mTab1.getTabGroupId()).thenReturn(GROUP_ID_1);

        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        FrameLayout groupView1 = new FrameLayout(activity);
        groupView1.layout(0, 0, 100, 48);

        boolean[] group1Hovered = new boolean[] {false};
        VerticalTabHoverController.setupTabGroupHeaderHover(
                mController.getTabHoverListener(),
                GROUP_HEADER_TAB_ID_1,
                /* tabGroupId= */ null,
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
        verify(mTabGroupHoverCardViewStub).inflate();
        verify(mTabGroupHoverCardView)
                .bindData(eq("Group 1"), eq(List.of("• Tab 1", "• Tab 2")), eq(0), eq(false));
        verify(mTabGroupHoverCardView).show(anyFloat(), anyFloat());
    }
}
