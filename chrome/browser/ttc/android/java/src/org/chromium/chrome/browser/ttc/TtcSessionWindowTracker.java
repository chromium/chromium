// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.WeakHashMap;

/**
 * Ends a profile's TTC session once no window that hosts the session UI is visible.
 *
 * <p>A session belongs to the profile, not to the window it was started from: voice focus and the
 * session UI follow the window the user is in. So one window stopping, closing or switching to
 * incognito must not end the session while another window is still visible. Microphone capture
 * can't continue in the background, so the session ends when the last visible window goes away.
 *
 * <p>A window reports here only once its native side is up, but the OS stops the previous window as
 * soon as a newly launched one is resumed, which can be before that. So when the last visible
 * window goes away while a browser window activity is visible that hasn't reported yet (for example
 * one the session itself just opened with create_window), the session is kept until that activity
 * either reports or stops. Every window reports once ready, whether or not it hosts the session UI,
 * so an incognito window isn't waited for once it is up.
 *
 * <p>There is one tracker per profile, owned by its {@link TtcKeyedService}.
 */
@NullMarked
class TtcSessionWindowTracker {
    private final Runnable mEndSession;

    // Windows hosting a visible session UI.
    private final Set<Object> mVisibleWindows = new HashSet<>();

    // Activities of windows whose native side is up, hosting the session UI or not. A visible
    // activity of the same type that isn't among them is still starting.
    private final Set<Activity> mReadyActivities = Collections.newSetFromMap(new WeakHashMap<>());

    // Set while the session is being kept alive for an activity that is still starting.
    private boolean mWaitingForWindow;

    private final ActivityStateListener mActivityStateListener =
            (activity, newState) -> {
                if (newState == ActivityState.STOPPED || newState == ActivityState.DESTROYED) {
                    endSessionUnlessWindowStarting();
                }
            };

    /**
     * @param endSession Ends the profile's session. Run when the last visible window goes away.
     */
    TtcSessionWindowTracker(Runnable endSession) {
        mEndSession = endSession;
    }

    /**
     * Records that the window with {@code activity} has its native side up, so it is no longer
     * waited for. A window that hosts the session UI must call {@link #addVisibleWindow} first if
     * it is visible, since this may otherwise end the session it was being kept alive for.
     */
    void onWindowReady(Activity activity) {
        ThreadUtils.assertOnUiThread();
        mReadyActivities.add(activity);
        if (mWaitingForWindow) endSessionUnlessWindowStarting();
    }

    /** Records that {@code window} is visible and hosting the session UI. Idempotent. */
    void addVisibleWindow(Object window) {
        ThreadUtils.assertOnUiThread();
        mVisibleWindows.add(window);
        setWaitingForWindow(false);
    }

    /**
     * Records that {@code window} no longer hosts a visible session UI, and ends the session if it
     * was the last one and no other browser window is still starting. No-op for a window that isn't
     * tracked.
     */
    void removeVisibleWindow(Object window) {
        ThreadUtils.assertOnUiThread();
        if (!mVisibleWindows.remove(window)) return;
        if (mVisibleWindows.isEmpty()) endSessionUnlessWindowStarting();
    }

    private void endSessionUnlessWindowStarting() {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (isVisible(activity) && isStartingHostActivity(activity)) {
                setWaitingForWindow(true);
                return;
            }
        }
        setWaitingForWindow(false);
        mEndSession.run();
    }

    private void setWaitingForWindow(boolean waiting) {
        if (waiting == mWaitingForWindow) return;
        mWaitingForWindow = waiting;
        if (waiting) {
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        } else {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
        }
    }

    /**
     * Whether {@code activity} is of a type that hosts the session UI but hasn't reported ready.
     * The type check runs both ways because the activities that host it are related by inheritance:
     * ChromeTabbedActivity2 extends ChromeTabbedActivity for pre-S multi-window, and a browser
     * test's first window is a subclass of the ChromeTabbedActivity that create_window opens.
     */
    private boolean isStartingHostActivity(Activity activity) {
        if (mReadyActivities.contains(activity)) return false;
        for (Activity ready : mReadyActivities) {
            if (ready.getClass().isInstance(activity) || activity.getClass().isInstance(ready)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isVisible(Activity activity) {
        @ActivityState int state = ApplicationStatus.getStateForActivity(activity);
        return state == ActivityState.STARTED
                || state == ActivityState.RESUMED
                || state == ActivityState.PAUSED;
    }
}
