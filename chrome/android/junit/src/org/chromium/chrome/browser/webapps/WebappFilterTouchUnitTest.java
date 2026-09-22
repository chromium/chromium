// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;
import android.view.MotionEvent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;

/**
 * Tests that the PWA/WebAPK windows filter touch events coming from an overlay activity, the same
 * way {@link org.chromium.chrome.browser.customtabs.CustomTabActivity} does.
 *
 * <p>These activities host installed web apps with no URL bar, so an overlay-routed tap would
 * otherwise reach an authenticated origin. See crbug.com/40063907 for the guard's origin.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class WebappFilterTouchUnitTest {
    private static MotionEvent createTouchDown() {
        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                MotionEvent.ACTION_DOWN,
                /* x= */ 0f,
                /* y= */ 0f,
                /* metaState= */ 0);
    }

    private static void setUpTracking(BaseCustomTabActivity activity) {
        // ApplicationStatus only starts tracking an activity on the CREATED transition.
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
    }

    private static void assertFiltersTouchesWhileNotResumed(BaseCustomTabActivity activity) {
        setUpTracking(activity);

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        assertFalse("Events should be accepted.", activity.shouldPreventTouch());
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
        assertTrue("Events should be discarded.", activity.shouldPreventTouch());

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
    }

    /**
     * Asserts that a touch arriving while the activity is PAUSED is consumed rather than forwarded
     * into the window, i.e. that {@code dispatchTouchEvent} is actually wired up to the guard.
     */
    private static void assertDiscardsTouchWhilePaused(BaseCustomTabActivity activity) {
        setUpTracking(activity);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);

        MotionEvent event = createTouchDown();
        try {
            assertTrue(
                    "Touch delivered while PAUSED should be discarded.",
                    activity.dispatchTouchEvent(event));
        } finally {
            event.recycle();
        }

        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
    }

    @Test
    public void testWebappActivityShouldPreventTouch() {
        assertFiltersTouchesWhileNotResumed(new WebappActivity());
    }

    @Test
    public void testSameTaskWebApkActivityShouldPreventTouch() {
        assertFiltersTouchesWhileNotResumed(new SameTaskWebApkActivity());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testWebappActivityDiscardsTouchWhilePaused_preT() {
        assertDiscardsTouchWhilePaused(new WebappActivity());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S_V2)
    public void testSameTaskWebApkActivityDiscardsTouchWhilePaused_preT() {
        assertDiscardsTouchWhilePaused(new SameTaskWebApkActivity());
    }
}
