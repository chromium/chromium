// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.verify;

import android.view.MotionEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

/** Tests {@link CustomTabActivity} filters touch events from overlay activity. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class CustomTabsFilterTouchUnitTest {
    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    @Mock private MotionEvent mMotionEvent;

    private CustomTabActivity mActivity;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    public void testShouldPreventTouch() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        assertFalse("Events should be accepted.", mActivity.shouldPreventTouch(mMotionEvent));
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        assertTrue("Events should be discarded.", mActivity.shouldPreventTouch(mMotionEvent));
    }

    @Test
    @SmallTest
    public void testInjectMissingEventInMultiWindowMode() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.PAUSED);
        assertTrue("Events should be consumed", mActivity.dispatchTouchEvent(mMotionEvent));

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        mActivity.onWindowFocusChanged(/* hasFocus= */ true);
        verify(mMotionEvent, atLeast(1)).setAction(eq(MotionEvent.ACTION_DOWN));
    }
}
