// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;

import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabHoverCardController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabHoverCardControllerUnitTest {

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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabRailLayout mContainerView;
    @Mock private ViewGroup mRootView;
    @Mock private View mTabView1;
    @Mock private View mTabView2;
    @Mock private View mPinnedTabView;
    @Mock private ViewGroup mViewStubParent;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mPinnedTab;

    private VerticalTabHoverCardController mController;

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
        when(mContainerView.getRootView()).thenReturn(mRootView);
        when(mRootView.getHeight()).thenReturn(ROOT_VIEW_HEIGHT_PX);
        when(mTabHoverCardView.getContext()).thenReturn(activity);
        when(mTabHoverCardView.getParent()).thenReturn(mRootView);
        when(mTabHoverCardView.getMeasuredHeight()).thenReturn(HOVER_CARD_VIEW_HEIGHT_PX);
        when(mTabHoverCardViewStub.getParent()).thenReturn(mViewStubParent);
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
                        invocation -> {
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

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mPinnedTab.getId()).thenReturn(PINNED_TAB_ID);
        when(mPinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getTabById(TAB_ID_2)).thenReturn(mTab2);
        when(mTabModelSelector.getTabById(PINNED_TAB_ID)).thenReturn(mPinnedTab);

        mController =
                new VerticalTabHoverCardController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        /* isContextMenuShowingSupplier= */ null);
    }

    // =========================================================================================
    // Show & Hide
    // =========================================================================================

    @Test
    @SmallTest
    public void testShowAndHide() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub).inflate();

        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        inOrder.verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testSelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testContextMenuShowing_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        VerticalTabHoverCardController controller =
                new VerticalTabHoverCardController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        () -> true);

        TabHoverCardListener listener = controller.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardViewStub, never()).inflate();
    }

    // =========================================================================================
    // Delay
    // =========================================================================================

    @Test
    @SmallTest
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
    @SmallTest
    public void testDelayedShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(
                TabHoverCardView.MIN_HOVER_CARD_DELAY_MS - 100, TimeUnit.MILLISECONDS);
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testExitBeforeDelay_CancelsShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);

        // Hover exit before delay expires
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Running remaining delayed tasks should not trigger show()
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    // =========================================================================================
    // Scrubbing Between Tabs
    // =========================================================================================

    @Test
    @SmallTest
    public void testScrubbing_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        // Hover tab 1 and wait for delay
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        clearInvocations(mTabHoverCardView);

        // Exit tab 1 hover (records exit time)
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Hover tab 2 within 300 ms buffer
        listener.onTabHoverCardStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);

        // Should show tab 2 immediately without needing ShadowLooper delay task flush
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testScrubbing_EnterBeforeExit_HidesBeforeShowingTab2() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        // Tab 1 is currently hovered and showing.
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Scrubbing: Tab 2 enters BEFORE Tab 1 exits (due to ViewGroup dispatch order).
        listener.onTabHoverCardStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab2));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Clear previous invocations to accurately verify the exit behavior.
        clearInvocations(mTabHoverCardView);

        // Tab 1 exits subsequently.
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Tab 2 should still be showing and hide() should NOT be called again.
        verify(mTabHoverCardView, never()).hide();
    }

    // =========================================================================================
    // Keyboard Focus
    // =========================================================================================

    @Test
    @SmallTest
    public void testKeyboardFocus_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Should show immediately for keyboard focus without needing ShadowLooper delay task flush.
        verify(mTabHoverCardViewStub).inflate();
        verify(mTabHoverCardView).show(anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testKeyboardFocus_FocusLost_HidesHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Inflation initializes and hides the view before showing.
        InOrder inOrder = inOrder(mTabHoverCardView);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).bindTab(eq(mTab1));
        inOrder.verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Focus lost triggers hide.
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);
        inOrder.verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testKeyboardFocus_SelectedTab_DoNotShowHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);
        when(mTabView1.hasFocus()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        assertNotNull(listener);

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(anyFloat(), anyFloat());
    }

    // =========================================================================================
    // Repositions
    // =========================================================================================

    @Test
    @SmallTest
    public void testCardHeightChange_RepositionsHoverCard() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);

        TabHoverCardListener listener = mController.getTabHoverCardListener();
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
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
    @SmallTest
    public void testGetHoverCardPosition_RegularTab_Collapsed() {
        when(mContainerView.getWidth()).thenReturn(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
    public void testGetHoverCardPosition_RegularTab_Expanded() {
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
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
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
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
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
    public void testGetHoverCardPosition_PinnedTab_Collapsed() {
        when(mContainerView.getWidth()).thenReturn(COLLAPSED_CONTAINER_WIDTH_PX);

        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
    public void testGetHoverCardPosition_PinnedTab_Expanded() {
        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
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
                VerticalTabHoverCardController.getHoverCardPosition(
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
    @SmallTest
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
                VerticalTabHoverCardController.getHoverCardPosition(
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
}
