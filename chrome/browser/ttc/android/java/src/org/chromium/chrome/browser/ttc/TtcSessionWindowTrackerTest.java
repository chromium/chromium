// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TtcSessionWindowTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TtcSessionWindowTrackerTest {
    /** Stands in for the production browser window activity. */
    public static class HostActivity extends Activity {}

    /** Stands in for a browser test's window activity, a subclass of the production one. */
    public static class TestHostActivity extends HostActivity {}

    /** An activity that never hosts the session UI, e.g. settings. */
    public static class OtherActivity extends Activity {}

    private final Object mWindowA = new Object();
    private final Object mWindowB = new Object();

    private int mEndSessionCount;
    private TtcSessionWindowTracker mTracker;

    @Before
    public void setUp() {
        mTracker = new TtcSessionWindowTracker(() -> mEndSessionCount++);
    }

    /** Creates an activity that {@link ApplicationStatus} tracks in {@code state}. */
    private static Activity createActivity(
            Class<? extends Activity> activityClass, @ActivityState int state) {
        // create() reports CREATED to ApplicationStatus, which must precede any other state.
        Activity activity = Robolectric.buildActivity(activityClass).create().get();
        if (state != ActivityState.CREATED) {
            ApplicationStatus.onStateChangeForTesting(activity, state);
        }
        return activity;
    }

    /** Adds {@code window} as a visible hosting window whose activity is a stopped host. */
    private void addStoppedHostWindow(Object window) {
        mTracker.addVisibleWindow(window);
        mTracker.onWindowReady(createActivity(HostActivity.class, ActivityState.STOPPED));
    }

    @Test
    public void testEndsSessionOnlyWhenLastWindowRemoved() {
        addStoppedHostWindow(mWindowA);
        addStoppedHostWindow(mWindowB);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testRemovingUntrackedWindowIsNoOp() {
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        addStoppedHostWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(0, mEndSessionCount);
    }

    @Test
    public void testRemovingWindowTwiceEndsSessionOnce() {
        addStoppedHostWindow(mWindowA);

        mTracker.removeVisibleWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testAddingWindowTwiceIsIdempotent() {
        addStoppedHostWindow(mWindowA);
        mTracker.addVisibleWindow(mWindowA);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testTracksWindowsAgainAfterSessionEnded() {
        addStoppedHostWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);

        mTracker.addVisibleWindow(mWindowA);
        addStoppedHostWindow(mWindowB);
        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(2, mEndSessionCount);
    }

    @Test
    public void testWindowWithoutActivityIsTracked() {
        mTracker.addVisibleWindow(mWindowA);
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    // The last window stops while a new browser window is visible but still starting, as when
    // the session itself opened the window: the session waits for it to register.
    @Test
    public void testWaitsForVisibleStartingHostActivity() {
        addStoppedHostWindow(mWindowA);
        Activity starting = createActivity(HostActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.addVisibleWindow(mWindowB);
        mTracker.onWindowReady(starting);
        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testEndsSessionWhenStartingActivityStops() {
        addStoppedHostWindow(mWindowA);
        Activity starting = createActivity(HostActivity.class, ActivityState.STARTED);

        mTracker.removeVisibleWindow(mWindowA);
        // Pausing is still visible, e.g. behind a dialog.
        ApplicationStatus.onStateChangeForTesting(starting, ActivityState.PAUSED);
        assertEquals(0, mEndSessionCount);

        ApplicationStatus.onStateChangeForTesting(starting, ActivityState.STOPPED);
        assertEquals(1, mEndSessionCount);

        // The wait is over; later activity changes don't end the session again.
        ApplicationStatus.onStateChangeForTesting(starting, ActivityState.DESTROYED);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testEndsSessionWhenStartingActivityDestroyed() {
        addStoppedHostWindow(mWindowA);
        Activity starting = createActivity(HostActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        ApplicationStatus.onStateChangeForTesting(starting, ActivityState.DESTROYED);
        assertEquals(1, mEndSessionCount);
    }

    // The window that was being waited for comes up but doesn't host the session, e.g. because it
    // is incognito: nothing is left to wait for.
    @Test
    public void testEndsSessionWhenStartingActivityIsReadyWithoutHosting() {
        addStoppedHostWindow(mWindowA);
        Activity starting = createActivity(HostActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.onWindowReady(starting);
        assertEquals(1, mEndSessionCount);
    }

    // A visible window that is up but doesn't host the session, e.g. an incognito window, or one
    // that stopped hosting it while still visible, isn't waited for.
    @Test
    public void testDoesNotWaitForReadyVisibleActivity() {
        Activity detaching = createActivity(HostActivity.class, ActivityState.RESUMED);
        mTracker.addVisibleWindow(mWindowA);
        mTracker.onWindowReady(detaching);
        addStoppedHostWindow(mWindowB);
        mTracker.onWindowReady(createActivity(HostActivity.class, ActivityState.RESUMED));
        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);

        mTracker.removeVisibleWindow(mWindowB);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testDoesNotWaitForOtherActivityTypes() {
        addStoppedHostWindow(mWindowA);
        createActivity(OtherActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    @Test
    public void testDoesNotWaitForInvisibleHostActivity() {
        addStoppedHostWindow(mWindowA);
        createActivity(HostActivity.class, ActivityState.CREATED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(1, mEndSessionCount);
    }

    // A browser test's window is a subclass of the production activity that a new window uses.
    @Test
    public void testWaitsForSuperclassOfHostActivity() {
        mTracker.addVisibleWindow(mWindowA);
        mTracker.onWindowReady(createActivity(TestHostActivity.class, ActivityState.STOPPED));
        createActivity(HostActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);
    }

    @Test
    public void testWaitsForSubclassOfHostActivity() {
        addStoppedHostWindow(mWindowA);
        createActivity(TestHostActivity.class, ActivityState.RESUMED);

        mTracker.removeVisibleWindow(mWindowA);
        assertEquals(0, mEndSessionCount);
    }
}
