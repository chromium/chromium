// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.SparseArray;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link WindowZOrderTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WindowZOrderTrackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mCallback;
    @Mock private Activity mActivity1;
    @Mock private Activity mActivity2;
    @Mock private ActivityWindowAndroid mWindowAndroid1;
    @Mock private ActivityWindowAndroid mWindowAndroid2;
    @Mock private DisplayAndroid mDisplay1;
    @Mock private DisplayAndroid mDisplay2;

    private WindowZOrderTracker mTracker;
    private static final int DISPLAY_ID_1 = 1;
    private static final int DISPLAY_ID_2 = 2;

    @Before
    public void setUp() {
        mTracker = new WindowZOrderTracker(mCallback);

        when(mWindowAndroid1.getActivity()).thenReturn(new WeakReference<>(mActivity1));
        when(mWindowAndroid1.getDisplay()).thenReturn(mDisplay1);
        when(mWindowAndroid1.getActivityState()).thenReturn(ActivityState.RESUMED);
        when(mDisplay1.getDisplayId()).thenReturn(DISPLAY_ID_1);

        when(mWindowAndroid2.getActivity()).thenReturn(new WeakReference<>(mActivity2));
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay2);
        when(mWindowAndroid2.getActivityState()).thenReturn(ActivityState.RESUMED);
        when(mDisplay2.getDisplayId()).thenReturn(DISPLAY_ID_2);
    }

    private void simulateTopResumed(ActivityWindowAndroid window, boolean isTopResumed) {
        ArgumentCaptor<WindowAndroid.ActivityStateObserver> captor =
                ArgumentCaptor.forClass(WindowAndroid.ActivityStateObserver.class);
        verify(window, atLeastOnce()).addActivityStateObserver(captor.capture());
        captor.getValue().onActivityTopResumedChanged(isTopResumed);
    }

    @Test
    public void testTrackSameWindow() {
        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid1);
        assertEquals(1, mTracker.getWindowZOrder().get(DISPLAY_ID_1).size());
    }

    @Test
    public void testUntrackUntrackedActivity() {
        mTracker.untrack(mWindowAndroid1);
        assertTrue(mTracker.getWindowZOrder().get(DISPLAY_ID_1) == null);
    }

    @Test
    public void testUntrackPreviouslyTrackedActivity() {
        mTracker.track(mWindowAndroid1);
        assertEquals(1, mTracker.getWindowZOrder().get(DISPLAY_ID_1).size());
        mTracker.untrack(mWindowAndroid1);
        assertTrue(mTracker.getWindowZOrder().get(DISPLAY_ID_1) == null);
    }

    @Test
    public void testOnTopResumedChangedRunsCallback() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1);
        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);

        simulateTopResumed(mWindowAndroid2, true);

        SparseArray<List<ActivityWindowAndroid>> zOrder = mTracker.getWindowZOrder();
        assertEquals(1, zOrder.size());
        List<ActivityWindowAndroid> display1Windows = zOrder.get(DISPLAY_ID_1);
        assertEquals(2, display1Windows.size());
        assertEquals(mWindowAndroid2, display1Windows.get(1));

        verify(mCallback).run();
    }

    @Test
    public void testTrackAlreadyTopResumedWindowRunsCallback() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1);
        mTracker.track(mWindowAndroid1);

        when(mWindowAndroid2.isTopResumedActivity()).thenReturn(true);
        mTracker.track(mWindowAndroid2);

        SparseArray<List<ActivityWindowAndroid>> zOrder = mTracker.getWindowZOrder();
        assertEquals(1, zOrder.size());
        List<ActivityWindowAndroid> display1Windows = zOrder.get(DISPLAY_ID_1);
        assertEquals(2, display1Windows.size());
        assertEquals(mWindowAndroid2, display1Windows.get(1));

        verify(mCallback).run();
    }

    @Test
    public void testZOrderWithMultipleActivities() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1); // Same display

        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);

        // Verify initial tracking state (both are in z-order, newest is at the bottom)
        List<ActivityWindowAndroid> zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid2, zOrder.get(0));
        assertEquals(mWindowAndroid1, zOrder.get(1));

        // Focus activity 1 (already at top, should be no-op)
        simulateTopResumed(mWindowAndroid1, true);
        zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid2, zOrder.get(0));
        assertEquals(mWindowAndroid1, zOrder.get(1));

        // Focus activity 2
        simulateTopResumed(mWindowAndroid2, true);
        zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid1, zOrder.get(0));
        assertEquals(mWindowAndroid2, zOrder.get(1));

        // Focus activity 1 again
        simulateTopResumed(mWindowAndroid1, true);
        zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid2, zOrder.get(0));
        assertEquals(mWindowAndroid1, zOrder.get(1));
    }

    @Test
    public void testRedundantPromotionIgnored() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1);
        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);

        // window 1 is at the top (index 1). Calling top resumed on it should not run callback.
        simulateTopResumed(mWindowAndroid1, true);

        verify(mCallback, never()).run();
    }

    @Test
    public void testPeriodicMetricsIncludesStoppedWindows() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.TrackedWindowsCount", 2)
                        .build();

        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);
        when(mWindowAndroid1.getActivityState()).thenReturn(ActivityState.STOPPED);

        ShadowLooper.idleMainLooper(5, TimeUnit.MINUTES);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testLockUnlockPreservesZOrder() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1); // Same display

        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);

        // Focus activity 1 so mWindowAndroid1 is on top (index 1)
        simulateTopResumed(mWindowAndroid1, true);

        // Verify active z-order
        List<ActivityWindowAndroid> zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid2, zOrder.get(0));
        assertEquals(ActivityState.RESUMED, zOrder.get(0).getActivityState());
        assertEquals(mWindowAndroid1, zOrder.get(1));
        assertEquals(ActivityState.RESUMED, zOrder.get(1).getActivityState());

        // Simulate locking device (update state to STOPPED, untrack is NOT called)
        when(mWindowAndroid1.getActivityState()).thenReturn(ActivityState.STOPPED);
        when(mWindowAndroid2.getActivityState()).thenReturn(ActivityState.STOPPED);

        zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(ActivityState.STOPPED, zOrder.get(0).getActivityState());
        assertEquals(ActivityState.STOPPED, zOrder.get(1).getActivityState());

        // Simulate unlocking device (update state to RESUMED)
        when(mWindowAndroid1.getActivityState()).thenReturn(ActivityState.RESUMED);
        when(mWindowAndroid2.getActivityState()).thenReturn(ActivityState.RESUMED);

        // Verify active z-order preserves previous relative order (mWindowAndroid1 on top)
        zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid2, zOrder.get(0));
        assertEquals(ActivityState.RESUMED, zOrder.get(0).getActivityState());
        assertEquals(mWindowAndroid1, zOrder.get(1));
        assertEquals(ActivityState.RESUMED, zOrder.get(1).getActivityState());
    }

    @Test
    public void testActivityDestroyedCleansUpHistory() {
        when(mWindowAndroid2.getDisplay()).thenReturn(mDisplay1); // Same display

        mTracker.track(mWindowAndroid1);
        mTracker.track(mWindowAndroid2);

        // Destroy activity 1 (untrack is called in onDestroy)
        mTracker.untrack(mWindowAndroid1);

        // Track activity 1 again (should be treated as a brand new window at index 0, because
        // history was cleaned up)
        mTracker.track(mWindowAndroid1);

        List<ActivityWindowAndroid> zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(2, zOrder.size());
        assertEquals(mWindowAndroid1, zOrder.get(0));
        assertEquals(mWindowAndroid2, zOrder.get(1));
    }

    @Test
    public void testSeparateDisplays() {
        mTracker.track(mWindowAndroid1); // Display 1
        mTracker.track(mWindowAndroid2); // Display 2 (setup in setUp)

        simulateTopResumed(mWindowAndroid1, true);
        simulateTopResumed(mWindowAndroid2, true);

        List<ActivityWindowAndroid> zOrder1 = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(1, zOrder1.size());
        assertEquals(mWindowAndroid1, zOrder1.get(0));

        List<ActivityWindowAndroid> zOrder2 = mTracker.getWindowZOrder().get(DISPLAY_ID_2);
        assertEquals(1, zOrder2.size());
        assertEquals(mWindowAndroid2, zOrder2.get(0));
    }

    @Test
    public void testIgnoreTopResumedFalse() {
        mTracker.track(mWindowAndroid1);
        simulateTopResumed(mWindowAndroid1, false);

        // Should not have promoted or run callback (it's already in z-order
        // at index 0 due to tracking).
        List<ActivityWindowAndroid> zOrder = mTracker.getWindowZOrder().get(DISPLAY_ID_1);
        assertEquals(1, zOrder.size());
        assertEquals(mWindowAndroid1, zOrder.get(0));
        verify(mCallback, never()).run();
    }

    @Test
    public void testPeriodicMetrics() {
        Activity activity3 = mock(Activity.class);
        ActivityWindowAndroid window3 = mock(ActivityWindowAndroid.class);
        when(window3.getActivity()).thenReturn(new WeakReference<>(activity3));
        when(window3.getDisplay()).thenReturn(mDisplay1);
        when(window3.getActivityState()).thenReturn(ActivityState.RESUMED);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.TrackedWindowsCount", 3)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.DisplaysCount", 2)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.PromotionCount", 2)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.ResumedCount", 3)
                        .build();

        mTracker.track(mWindowAndroid1); // on display 1 (top)
        mTracker.track(window3); // on display 1 (bottom)
        simulateTopResumed(
                window3, true); // promotes window3 (resumed count = 1, promotion count = 1)
        simulateTopResumed(
                mWindowAndroid1, true); // promotes window1 (resumed count = 2, promotion count = 2)

        mTracker.track(mWindowAndroid2); // on display 2
        simulateTopResumed(
                mWindowAndroid2,
                true); // promotes window2 (resumed count = 3, promotion count = 2 ignored because
        // it was already top)

        ShadowLooper.idleMainLooper(5, TimeUnit.MINUTES);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testPeriodicMetricsEmpty() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.TrackedWindowsCount", 0)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.DisplaysCount", 0)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.PromotionCount", 0)
                        .expectIntRecord("Android.MultiWindow.WindowZOrder.ResumedCount", 0)
                        .build();

        ShadowLooper.idleMainLooper(5, TimeUnit.MINUTES);

        histogramWatcher.assertExpected();
    }
}
