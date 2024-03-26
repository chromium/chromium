// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.desktop_windowing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetsRectProvider;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit test for {@link AppHeaderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class AppHeaderCoordinatorUnitTest {
    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;
    private static final Rect WINDOW_RECT = new Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    private static final int LEFT_BLOCK = 10;
    private static final int RIGHT_BLOCK = 20;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private View mRootView;
    @Mock private StripLayoutHelperManager mStripLayoutManager;
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private InsetsRectProvider mInsetsRectProvider;
    @Captor private ArgumentCaptor<InsetsRectProvider.Observer> mInsetRectObserverCaptor;

    private AppHeaderCoordinator mAppHeaderCoordinator;
    private Activity mSpyActivity;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mSpyActivity = spy(activity));
        doReturn(true).when(mSpyActivity).isInMultiWindowMode();
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);
        setupWithNoInsets();
        initAppHeaderCoordinator();
    }

    @Test
    public void notEnabledWithNoTopInsets() {
        // Bottom insets with height = 30
        Insets bottomInsets = Insets.of(0, 0, 0, 30);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 30, LEFT_BLOCK, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - RIGHT_BLOCK,
                                WINDOW_HEIGHT - 30,
                                LEFT_BLOCK,
                                WINDOW_HEIGHT));
        Rect widestUnOccludedRect =
                new Rect(LEFT_BLOCK, WINDOW_HEIGHT - 30, WINDOW_WIDTH - RIGHT_BLOCK, WINDOW_HEIGHT);
        setupInsetsRectProvider(bottomInsets, blockedRects, widestUnOccludedRect, WINDOW_RECT);

        assertFalse(
                "Desktop Windowing not enabled for bottom insets.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
    }

    @Test
    public void notEnabledWithBoundingRectsWithPartialHeight() {
        // Bottom insets with height = 30
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, right block: 20. Bounding rect is not at the full height.
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, RIGHT_BLOCK),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH, 20));
        Rect widestUnoccludedRect = new Rect(0, 20, WINDOW_WIDTH, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);

        assertFalse(
                "Desktop Windowing enabled for widestUnOccludedRect with less height "
                        + " than the insets.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
    }

    @Test
    public void notEnabledWithLessThanTwoBoundingRects() {
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10
        List<Rect> blockedRects = List.of(new Rect(0, 0, LEFT_BLOCK, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);

        assertFalse(
                "Desktop Windowing enabled with only one bounding rect.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
    }

    @Test
    public void notEnabledWithMoreThanTwoBoundingRects() {
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, two right block: 5, 15
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, 30),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH - 15, 30),
                        new Rect(WINDOW_WIDTH - 15, 0, WINDOW_WIDTH, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);

        assertFalse(
                "Desktop Windowing enabled with more than two bounding rects.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
    }

    @Test
    public void notEnabledWhenNotInMultiWindowMode() {
        doReturn(false).when(mSpyActivity).isInMultiWindowMode();
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing does not enable when not in multi window mode.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
    }

    @Test
    public void enableDesktopWindowing() {
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();
        verify(mStripLayoutManager)
                .updateHorizontalPaddings(eq((float) LEFT_BLOCK), eq((float) RIGHT_BLOCK));
    }

    @Test
    public void changeBoundingRects() {
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Assume the window size changed.
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        int newWindowWidth = 1000;
        Rect windowRect = new Rect(0, 0, newWindowWidth, WINDOW_HEIGHT);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, 30),
                        new Rect(newWindowWidth - RIGHT_BLOCK, 0, newWindowWidth, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, newWindowWidth - RIGHT_BLOCK, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, windowRect);
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();
        verify(mStripLayoutManager, times(2))
                .updateHorizontalPaddings(eq((float) LEFT_BLOCK), eq((float) RIGHT_BLOCK));
    }

    @Test
    public void initializeWithDesktopWindowingThenExit() {
        setupWithLeftAndRightBoundingRect();
        initAppHeaderCoordinator();
        verifyDesktopWindowingEnabled();
        verify(mStripLayoutManager)
                .updateHorizontalPaddings(eq((float) LEFT_BLOCK), eq((float) RIGHT_BLOCK));

        setupWithNoInsets();
        notifyInsetsRectObserver();
        assertFalse(
                "DesktopWindowing should exit when no insets is supplied.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
        verify(mStripLayoutManager).updateHorizontalPaddings(eq(0f), eq(0f));
        verify(mBrowserControlsVisDelegate).releasePersistentShowingToken(anyInt());
    }

    @Test
    public void testDestroy() {
        mAppHeaderCoordinator.destroy();

        verify(mInsetsRectProvider).removeObserver(any());
        verify(mRootView, times(0)).setSystemGestureExclusionRects(any());
        verify(mStripLayoutManager, times(0)).updateHorizontalPaddings(anyFloat(), anyFloat());
    }

    private void initAppHeaderCoordinator() {
        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        mSpyActivity,
                        mRootView,
                        mStripLayoutManager,
                        mBrowserControlsVisDelegate,
                        mInsetObserver);
    }

    private void setupWithNoInsets() {
        setupInsetsRectProvider(Insets.NONE, List.of(), new Rect(), WINDOW_RECT);
    }

    private void setupWithLeftAndRightBoundingRect() {
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, 30),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
    }

    private void setupInsetsRectProvider(
            Insets insets, List<Rect> blockedRects, Rect widestUnOccludedRect, Rect windowRect) {
        doReturn(windowRect).when(mInsetsRectProvider).getWindowRect();
        doReturn(widestUnOccludedRect).when(mInsetsRectProvider).getWidestUnoccludedRect();
        doReturn(insets).when(mInsetsRectProvider).getCachedInset();
        doReturn(blockedRects).when(mInsetsRectProvider).getBoundingRects();
    }

    private void notifyInsetsRectObserver() {
        verify(mInsetsRectProvider, atLeastOnce()).addObserver(mInsetRectObserverCaptor.capture());
        mInsetRectObserverCaptor
                .getValue()
                .onBoundingRectsUpdated(mInsetsRectProvider.getWidestUnoccludedRect());
    }

    private void verifyDesktopWindowingEnabled() {
        assertTrue(
                "Desktop windowing not enabled.",
                mAppHeaderCoordinator.isDesktopWindowingEnabled());
        verify(mBrowserControlsVisDelegate, atLeastOnce())
                .showControlsPersistentAndClearOldToken(anyInt());
        verify(mRootView, atLeastOnce()).setSystemGestureExclusionRects(any());
    }
}
