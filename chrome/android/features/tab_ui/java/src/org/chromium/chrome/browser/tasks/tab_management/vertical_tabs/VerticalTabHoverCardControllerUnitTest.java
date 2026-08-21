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

import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabHoverCardController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabHoverCardControllerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabRailLayout mContainerView;
    @Mock private View mRootView;
    @Mock private View mTabView1;
    @Mock private View mTabView2;
    @Mock private ViewGroup mViewStubParent;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    private VerticalTabHoverCardController mController;

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int ROOT_VIEW_HEIGHT_PX = 1000;
    private static final int EXPANDED_CONTAINER_WIDTH_PX = 240;
    private static final int COLLAPSED_CONTAINER_WIDTH_PX = 76;
    private static final int PINNED_TAB_VIEW_HEIGHT_PX = 40;
    private static final int HOVER_CARD_VIEW_HEIGHT_PX = 200;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mContainerView.getContext()).thenReturn(activity);
        when(mContainerView.getRootView()).thenReturn(mRootView);
        when(mRootView.getHeight()).thenReturn(ROOT_VIEW_HEIGHT_PX);
        when(mTabHoverCardView.getContext()).thenReturn(activity);
        when(mTabHoverCardView.getMeasuredHeight()).thenReturn(HOVER_CARD_VIEW_HEIGHT_PX);
        when(mTabHoverCardViewStub.getParent()).thenReturn(mViewStubParent);

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
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(mTab1);
        when(mTabModelSelector.getTabById(TAB_ID_2)).thenReturn(mTab2);

        mController =
                new VerticalTabHoverCardController(
                        mContainerView,
                        mTabHoverCardViewStub,
                        mTabModelSelector,
                        mTabContentManagerSupplier,
                        /* isContextMenuShowingSupplier= */ null);
    }

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
        inOrder.verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());

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
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testDelayedShow() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);

        // Before delay elapses (200 ms), show should not be called yet.
        ShadowLooper.idleMainLooper(200, TimeUnit.MILLISECONDS);
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());

        // After full delay (300 ms), show is called.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());
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
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testScrubbing_ShowsImmediately() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_3);
        when(mTabHoverCardView.isShown()).thenReturn(true);

        TabHoverCardListener listener = mController.getTabHoverCardListener();

        // Hover tab 1 and wait for delay
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());

        // Exit tab 1 hover (records exit time)
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Hover tab 2 within 300 ms buffer
        listener.onTabHoverCardStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);

        // Should show tab 2 immediately without needing ShadowLooper delay task flush
        verify(mTabHoverCardView).show(eq(mTab2), anyFloat(), anyFloat());
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
        inOrder.verify(mTabHoverCardView).show(eq(mTab1), anyFloat(), anyFloat());

        // Scrubbing: Tab 2 enters BEFORE Tab 1 exits (due to ViewGroup dispatch order).
        listener.onTabHoverCardStateChanged(TAB_ID_2, mTabView2, /* isHovered= */ true);
        inOrder.verify(mTabHoverCardView).hide();
        inOrder.verify(mTabHoverCardView).show(eq(mTab2), anyFloat(), anyFloat());

        // Clear previous invocations to accurately verify the exit behavior.
        clearInvocations(mTabHoverCardView);

        // Tab 1 exits subsequently.
        listener.onTabHoverCardStateChanged(TAB_ID_1, mTabView1, /* isHovered= */ false);

        // Tab 2 should still be showing and hide() should NOT be called again.
        verify(mTabHoverCardView, never()).hide();
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
        assertEquals((float) EXPANDED_CONTAINER_WIDTH_PX, position[0], 0.01f);
        assertEquals(0f, position[1], 0.01f);
    }

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
        assertEquals((float) COLLAPSED_CONTAINER_WIDTH_PX, position[0], 0.01f);
        assertEquals(0f, position[1], 0.01f);
    }

    @Test
    @SmallTest
    public void testGetHoverCardPosition_PinnedTab_Expanded() {
        when(mTabView1.getHeight()).thenReturn(PINNED_TAB_VIEW_HEIGHT_PX);

        float[] position =
                VerticalTabHoverCardController.getHoverCardPosition(
                        mTabView1,
                        mContainerView,
                        mTabHoverCardView,
                        /* isPinnedTab= */ true,
                        /* isRailCollapsed= */ false);

        assertEquals(2, position.length);
        assertEquals(0f, position[0], 0.01f);
        assertEquals((float) PINNED_TAB_VIEW_HEIGHT_PX, position[1], 0.01f);
    }

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
        assertEquals((float) COLLAPSED_CONTAINER_WIDTH_PX, position[0], 0.01f);
        assertEquals(0f, position[1], 0.01f);
    }

    @Test
    @SmallTest
    public void testGetHoverCardPosition_ExceedsRootHeight_ClampsToParentBounds() {
        when(mContainerView.getWidth()).thenReturn(EXPANDED_CONTAINER_WIDTH_PX);
        // Position the tab view near the bottom of the window so that the hover card extends
        // beyond the root view height by 10px (relativeY + hoverCardHeight > parentHeight).
        doAnswer(
                        invocation -> {
                            int[] array = invocation.getArgument(0);
                            array[1] = ROOT_VIEW_HEIGHT_PX - HOVER_CARD_VIEW_HEIGHT_PX + 10;
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
        assertEquals((float) EXPANDED_CONTAINER_WIDTH_PX, position[0], 0.01f);
        // The hover card should be shifted upward to clamp to the root view bottom boundary:
        // hoverCardY = ROOT_VIEW_HEIGHT_PX - HOVER_CARD_VIEW_HEIGHT_PX.
        assertEquals((float) (ROOT_VIEW_HEIGHT_PX - HOVER_CARD_VIEW_HEIGHT_PX), position[1], 0.01f);
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
}
