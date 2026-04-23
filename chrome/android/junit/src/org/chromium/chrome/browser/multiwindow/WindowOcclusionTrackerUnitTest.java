// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Rect;
import android.util.SparseArray;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.display.DisplayAndroid;

import java.time.Duration;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link WindowOcclusionTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class WindowOcclusionTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowZOrderTracker mZOrderTracker;

    private WindowOcclusionTracker mOcclusionTracker;
    private static final int DISPLAY_ID = 1;
    private static final int DISPLAY_WIDTH = 1080;
    private static final int DISPLAY_HEIGHT = 1920;

    @Before
    public void setUp() {
        UiAndroidFeatureList.sAndroidWindowOcclusionMinimumVisibilitySizeThreshold.setForTesting(0);
        UiAndroidFeatureList.sAndroidWindowOcclusionCalculateOcclusionRateLimitMs.setForTesting(0);
        mOcclusionTracker = new WindowOcclusionTracker(mZOrderTracker);
    }

    @After
    public void tearDown() {
        WindowOcclusionMetrics.resetForTesting();
    }

    private View createView(int x, int y, int width, int height) {
        View view = new View(ContextUtils.getApplicationContext());
        ShadowView shadowView = shadowOf(view);
        // Set global visible rect relative to the window (root view).
        shadowView.setGlobalVisibleRect(new Rect(x, y, x + width, y + height));
        return view;
    }

    private ActivityWindowAndroid createWindowAndroid(View view) {
        ActivityWindowAndroid window = org.mockito.Mockito.mock(ActivityWindowAndroid.class);
        android.view.Window androidWindow = org.mockito.Mockito.mock(android.view.Window.class);
        when(window.getWindow()).thenReturn(androidWindow);
        when(androidWindow.getDecorView()).thenReturn(view);

        DisplayAndroid displayAndroid = org.mockito.Mockito.mock(DisplayAndroid.class);
        when(displayAndroid.getDisplayWidth()).thenReturn(DISPLAY_WIDTH);
        when(displayAndroid.getDisplayHeight()).thenReturn(DISPLAY_HEIGHT);
        when(window.getDisplay()).thenReturn(displayAndroid);

        return window;
    }

    @Test
    public void testSingleViewVisible() {
        View view = createView(0, 0, 100, 100);
        ActivityWindowAndroid window = createWindowAndroid(view);
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Collections.singletonList(window));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(window, description("Window should not be occluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testOcclusionCalculationsMetric() {
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.MultiWindow.Occlusion.OcclusionCalculationsPer5Minutes", 2)
                        .build();

        mOcclusionTracker.calculateOcclusion();
        mOcclusionTracker.calculateOcclusion();

        ShadowSystemClock.advanceBy(Duration.ofMinutes(5));
        ShadowLooper.idleMainLooper();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testCalculateDurationMetric() {
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        when(mZOrderTracker.getWindowZOrder())
                .thenAnswer(
                        invocation -> {
                            ShadowSystemClock.advanceBy(Duration.ofMillis(10));
                            return zOrder;
                        });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.Occlusion.CalculateDuration", 10)
                        .build();

        mOcclusionTracker.calculateOcclusion();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testSingleViewNoRectDefaultsUnoccluded() {
        // View outside of display bounds
        View view = createView(DISPLAY_WIDTH + 10, 0, 100, 100);
        ActivityWindowAndroid window = createWindowAndroid(view);
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Collections.singletonList(window));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(window, description("Window should default to unoccluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testTwoViewsNoOverlap() {
        View view1 = createView(0, 0, 100, 100);
        ActivityWindowAndroid window1 = createWindowAndroid(view1);
        View view2 = createView(200, 200, 100, 100);
        ActivityWindowAndroid window2 = createWindowAndroid(view2);

        // view2 is on top of view1 (higher index in list)
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(window1, window2));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(window2, description("Top window should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(window1, description("Bottom window should not be occluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testTwoViewsFullOcclusion() {
        View bottomView = createView(0, 0, 100, 100);
        ActivityWindowAndroid bottomWindow = createWindowAndroid(bottomView);
        View topView = createView(0, 0, 100, 100); // Covers bottomView completely
        ActivityWindowAndroid topWindow = createWindowAndroid(topView);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow, topWindow));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(topWindow, description("Top window should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(bottomWindow, description("Bottom window should be occluded"))
                .setOccluded(eq(true), any(), any());
    }

    @Test
    public void testTwoViewsPartialOcclusion() {
        View bottomView = createView(0, 0, 100, 100);
        ActivityWindowAndroid bottomWindow = createWindowAndroid(bottomView);
        View topView = createView(50, 50, 100, 100); // Partially covers bottomView
        ActivityWindowAndroid topWindow = createWindowAndroid(topView);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow, topWindow));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusion();

        verify(topWindow, description("Top window should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(bottomWindow, description("Bottom window should not be occluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testMultipleCoveringViews() {
        View bottomView = createView(0, 0, 100, 100);
        ActivityWindowAndroid bottomWindow = createWindowAndroid(bottomView);
        // Two views that together cover the bottom view
        View topView1 = createView(0, 0, 50, 100);
        ActivityWindowAndroid topWindow1 = createWindowAndroid(topView1);
        View topView2 = createView(50, 0, 50, 100);
        ActivityWindowAndroid topWindow2 = createWindowAndroid(topView2);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow, topWindow1, topWindow2));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(topWindow1, description("Top window 1 should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(topWindow2, description("Top window 2 should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(bottomWindow, description("Bottom window should be fully occluded by combination"))
                .setOccluded(eq(true), any(), any());
    }

    @Test
    public void testDisplayNotFound() {
        View view = createView(0, 0, 100, 100);
        ActivityWindowAndroid window = createWindowAndroid(view);
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(999, Collections.singletonList(window));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        verify(window, description("Window on unknown display should default to unoccluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testMultipleDisplays() {
        int displayId2 = 2;

        // Display 1: Two overlapping views (bottom occluded)
        View bottomView1 = createView(0, 0, 100, 100);
        ActivityWindowAndroid bottomWindow1 = createWindowAndroid(bottomView1);
        View topView1 = createView(0, 0, 100, 100);
        ActivityWindowAndroid topWindow1 = createWindowAndroid(topView1);

        // Display 2: One visible view
        View view2 = createView(0, 0, 100, 100);
        ActivityWindowAndroid window2 = createWindowAndroid(view2);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow1, topWindow1));
        zOrder.put(displayId2, Collections.singletonList(window2));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();

        // Display 1 assertions
        verify(topWindow1, description("Top window on Display 1 should not be occluded"))
                .setOccluded(eq(false), any(), any());
        verify(bottomWindow1, description("Bottom window on Display 1 should be occluded"))
                .setOccluded(eq(true), any(), any());

        // Display 2 assertions
        verify(window2, description("Window on Display 2 should not be occluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testVisibleDimensionThreshold() {
        // Threshold of 10 pixels
        UiAndroidFeatureList.sAndroidWindowOcclusionMinimumVisibilitySizeThreshold.setForTesting(
                10);

        // Recreate the test instance to grab the overridden threshold.
        mOcclusionTracker = new WindowOcclusionTracker(mZOrderTracker);

        // Bottom view: 100x100
        View bottomView = createView(0, 0, 100, 100);
        ActivityWindowAndroid bottomWindow = createWindowAndroid(bottomView);

        // Top view: 100x91. Leaves 100x9 visible at the bottom. Height = 9.
        // If threshold is 10, this should be occluded.
        View topView1 = createView(0, 0, 100, 91);
        ActivityWindowAndroid topWindow1 = createWindowAndroid(topView1);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow, topWindow1));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();
        verify(bottomWindow, description("Window with visible height 9 should be occluded"))
                .setOccluded(eq(true), any(), any());

        // Now test with visible height 10.
        // Bottom view: 0,0 - 100,100
        // Top view: 0,0 - 100,90. Visible rect: 0,90 - 100,100. Height = 10.
        View topView2 = createView(0, 0, 100, 90);
        ActivityWindowAndroid topWindow2 = createWindowAndroid(topView2);

        // Clear invocations so we can verify the second call without losing stubs
        org.mockito.Mockito.clearInvocations(bottomWindow);

        zOrder.put(DISPLAY_ID, Arrays.asList(bottomWindow, topWindow2));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        mOcclusionTracker.calculateOcclusionRateLimited();
        verify(bottomWindow, description("Window with visible height 10 should NOT be occluded"))
                .setOccluded(eq(false), any(), any());
    }

    @Test
    public void testTrack_ViewNotFound() {
        ActivityWindowAndroid window = org.mockito.Mockito.mock(ActivityWindowAndroid.class);
        when(mZOrderTracker.track(window)).thenReturn(true);
        // window.getWindow() returns null by default.

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.MultiWindow.Occlusion.TrackResult", false)
                        .build();

        mOcclusionTracker.track(window);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testUntrack_ViewNotFound() {
        ActivityWindowAndroid window = org.mockito.Mockito.mock(ActivityWindowAndroid.class);
        when(mZOrderTracker.untrack(window)).thenReturn(true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Android.MultiWindow.Occlusion.UntrackResult", false)
                        .build();

        mOcclusionTracker.untrack(window);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testCalculateOcclusion_ViewNotFound() {
        // 2 windows are required for calculateOcclusion() to do any work.
        ActivityWindowAndroid window1 = createWindowAndroid(createView(0, 0, 100, 100));
        ActivityWindowAndroid window2 = org.mockito.Mockito.mock(ActivityWindowAndroid.class);
        DisplayAndroid displayAndroid = window1.getDisplay();
        when(window2.getDisplay()).thenReturn(displayAndroid);

        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Arrays.asList(window1, window2));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.MultiWindow.Occlusion.CalculateResult",
                                WindowOcclusionMetrics.CalculateResult.SUCCESS,
                                WindowOcclusionMetrics.CalculateResult.VIEW_NOT_FOUND)
                        .build();

        mOcclusionTracker.calculateOcclusionRateLimited();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testCalculateOcclusionRateLimit() {
        UiAndroidFeatureList.sAndroidWindowOcclusionCalculateOcclusionRateLimitMs.setForTesting(
                100);

        // Recreate the test instance to grab the overridden rate limit.
        mOcclusionTracker = new WindowOcclusionTracker(mZOrderTracker);

        View view = createView(0, 0, 100, 100);
        ActivityWindowAndroid window = createWindowAndroid(view);
        SparseArray<List<ActivityWindowAndroid>> zOrder = new SparseArray<>();
        zOrder.put(DISPLAY_ID, Collections.singletonList(window));
        when(mZOrderTracker.getWindowZOrder()).thenReturn(zOrder);

        // First call should execute immediately.
        mOcclusionTracker.calculateOcclusionRateLimited();
        verify(window, description("First call should execute immediately."))
                .setOccluded(eq(false), any(), any());

        // Second call within rate limit should be delayed.
        org.mockito.Mockito.clearInvocations(window);
        mOcclusionTracker.calculateOcclusionRateLimited();
        verify(window, org.mockito.Mockito.never().description("Second call should be delayed."))
                .setOccluded(anyBoolean(), any(), any());

        // Fast forward time by 100ms.
        ShadowSystemClock.advanceBy(Duration.ofMillis(100));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Now the delayed task should have executed.
        verify(window, description("Delayed task should have executed."))
                .setOccluded(eq(false), any(), any());
    }
}
