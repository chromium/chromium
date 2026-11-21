// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;
import android.view.MotionEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BaseSwitches;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Verifies that {@link LightweightFirstRunActivity} drops touches while it is not resumed. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.S)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    BaseSwitches.DISABLE_NATIVE_INITIALIZATION,
})
public class LightweightFirstRunFilterTouchUnitTest {
    private ActivityController<LightweightFirstRunActivity> mController;
    private LightweightFirstRunActivity mActivity;

    @Before
    public void setUp() {
        mController = Robolectric.buildActivity(LightweightFirstRunActivity.class);
        mActivity = mController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mController.create();
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        // The activity never inflates its views without native, so destroy() is not exercised.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    @Test
    public void testShouldPreventTouch() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        assertFalse("Events should be accepted.", mActivity.shouldPreventTouch());
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        assertTrue("Events should be discarded.", mActivity.shouldPreventTouch());
    }

    @Test
    public void testDispatchTouchEvent_consumedWhilePaused() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        MotionEvent down =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 1,
                        /* y= */ 1,
                        /* metaState= */ 0);
        try {
            assertTrue("Events should be consumed.", mActivity.dispatchTouchEvent(down));
        } finally {
            down.recycle();
        }
    }
}
