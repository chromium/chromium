// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub.swipe;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Looper;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubPaneHostView.InteractiveElementChecker;
import org.chromium.chrome.browser.hub.HubPaneHostView.PaneViewProvider;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link HubPaneSwipeCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.ENABLE_SWIPE_TO_SWITCH_PANE)
public class HubPaneSwipeCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneViewProvider mPaneViewProvider;
    @Mock private InteractiveElementChecker mInteractiveChecker;

    private ActivityController<TestActivity> mActivityController;
    private FrameLayout mHostView;
    private FrameLayout mPaneFrame;
    private HubPaneSwipeCoordinator mCoordinator;

    private static final int CONTAINER_WIDTH = 1000;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        Activity activity = mActivityController.get();

        mHostView = new FrameLayout(activity);
        mPaneFrame = new FrameLayout(activity);
        mHostView.addView(mPaneFrame);

        mHostView.setLayoutParams(new FrameLayout.LayoutParams(CONTAINER_WIDTH, 1000));
        mPaneFrame.setLayoutParams(new FrameLayout.LayoutParams(CONTAINER_WIDTH, 1000));

        mHostView.measure(
                View.MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mHostView.layout(0, 0, CONTAINER_WIDTH, 1000);
        mPaneFrame.measure(
                View.MeasureSpec.makeMeasureSpec(CONTAINER_WIDTH, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        mPaneFrame.layout(0, 0, CONTAINER_WIDTH, 1000);

        mCoordinator = new HubPaneSwipeCoordinator(mHostView, mPaneFrame);
        mCoordinator.setPaneViewProvider(mPaneViewProvider);
        mCoordinator.setInteractiveElementChecker(mInteractiveChecker);
    }

    @After
    public void tearDown() {
        mCoordinator.destroy();
        mActivityController.close();
    }

    @Test
    public void testSetRootView_initialSetup() {
        Activity activity = mActivityController.get();
        View rootView = new View(activity);

        mCoordinator.setRootView(rootView, false);

        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(rootView, mPaneFrame.getChildAt(0));
    }

    @Test
    public void testSetRootView_nullClearsChildren() {
        Activity activity = mActivityController.get();
        View rootView = new View(activity);
        mCoordinator.setRootView(rootView, false);
        assertEquals(1, mPaneFrame.getChildCount());

        mCoordinator.setRootView(null, false);
        assertEquals(0, mPaneFrame.getChildCount());
    }

    @Test
    public void testOnSwipeStarted_preparesAdjacentView() {
        Activity activity = mActivityController.get();
        View adjacentView = new View(activity);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);

        View result = mCoordinator.onSwipeStarted(true);

        assertNotNull(result);
        assertEquals(adjacentView, result);
        assertEquals(CONTAINER_WIDTH, adjacentView.getTranslationX(), 0.01f);
        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(adjacentView, mPaneFrame.getChildAt(0));
    }

    @Test
    public void testOnSwipeProgress_updatesTranslationsAndNotifiesProvider() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(true);

        mCoordinator.onSwipeProgress(-300f, 0.3f, true);

        assertEquals(-300f, currentView.getTranslationX(), 0.01f);
        assertEquals(700f, adjacentView.getTranslationX(), 0.01f);
        verify(mPaneViewProvider).onSwipeDragProgress(eq(0.3f), eq(true));
    }

    @Test
    public void testOnSwipeSettled_switchSuccess() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(true);
        mCoordinator.onSwipeProgress(-400f, 0.4f, true);

        mCoordinator.onSwipeSettled(/* shouldSwitch= */ true, /* isSwipeLeft= */ true);

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        verify(mPaneViewProvider).onSwipeSwitchComplete(true);

        // When mediator reacts to complete and sets new root view:
        mCoordinator.setRootView(adjacentView, false);
        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(adjacentView, mPaneFrame.getChildAt(0));
        assertEquals(0f, adjacentView.getTranslationX(), 0.01f);
    }

    @Test
    public void testOnSwipeSettled_cancelRestoresViews() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(true);
        mCoordinator.onSwipeProgress(-100f, 0.1f, true);

        mCoordinator.onSwipeSettled(/* shouldSwitch= */ false, /* isSwipeLeft= */ true);

        Shadows.shadowOf(Looper.getMainLooper()).idleFor(1, TimeUnit.SECONDS);

        verify(mPaneViewProvider).onSwipeSwitchCancel(true);
        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(currentView, mPaneFrame.getChildAt(0));
        assertEquals(0f, currentView.getTranslationX(), 0.01f);
    }

    @Test
    public void testOnSwipeCancelled_removesAdjacentAndResetsTranslation() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(true);
        mCoordinator.onSwipeProgress(-100f, 0.1f, true);

        mCoordinator.onSwipeCancelled();

        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(currentView, mPaneFrame.getChildAt(0));
        assertEquals(0f, currentView.getTranslationX(), 0.01f);
        verify(mPaneViewProvider).onSwipeDragProgress(eq(0.0f), eq(true));
        verify(mPaneViewProvider).onSwipeSwitchCancel(eq(true));
    }

    @Test
    public void testOnSwipeCancelled_swipeRight_notifiesCancelWithFalse() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(false)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(false);
        mCoordinator.onSwipeProgress(100f, 0.1f, false);

        mCoordinator.onSwipeCancelled();

        assertEquals(1, mPaneFrame.getChildCount());
        assertEquals(currentView, mPaneFrame.getChildAt(0));
        assertEquals(0f, currentView.getTranslationX(), 0.01f);
        verify(mPaneViewProvider).onSwipeDragProgress(eq(0.0f), eq(false));
        verify(mPaneViewProvider).onSwipeSwitchCancel(eq(false));
    }

    @Test
    public void testIsTouchOnInteractiveElement() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        mCoordinator.setRootView(currentView, false);

        when(mInteractiveChecker.isTouchOnInteractiveElement(anyFloat(), anyFloat()))
                .thenReturn(true);
        assertTrue(mCoordinator.isTouchOnInteractiveElement(200f, 300f));

        when(mInteractiveChecker.isTouchOnInteractiveElement(anyFloat(), anyFloat()))
                .thenReturn(false);
        assertFalse(mCoordinator.isTouchOnInteractiveElement(200f, 300f));
    }

    @Test
    public void testOnSwipeSettled_switchSuccess_cancelledBeforeNewRoot_resetsInteractiveSwitch() {
        Activity activity = mActivityController.get();
        View currentView = new View(activity);
        View adjacentView = new View(activity);

        mCoordinator.setRootView(currentView, false);
        when(mPaneViewProvider.prepareAndGetAdjacentPaneView(true)).thenReturn(adjacentView);
        mCoordinator.onSwipeStarted(true);
        mCoordinator.onSwipeProgress(-400f, 0.4f, true);

        // Starts settle with shouldSwitch = true, setting mIsInteractiveSwitchInProgress = true.
        mCoordinator.onSwipeSettled(/* shouldSwitch= */ true, /* isSwipeLeft= */ true);

        // Cancelled before new root is delivered.
        mCoordinator.onSwipeCancelled();

        // If next setRootView happens with old and new view, it should animate regular slide
        // rather than doing instant interactive switch swap.
        View nextView = new View(activity);
        mCoordinator.setRootView(nextView, true);
        // Both old and new views will be in paneFrame during slide transition rather than immediate
        // removal.
        assertEquals(2, mPaneFrame.getChildCount());
    }
}
