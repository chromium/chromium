// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link CustomTabActivityTimeoutHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED})
public class CustomTabActivityTimeoutHandlerUnitTest {
    @Mock private Runnable mFinishRunnable;
    @Mock private PendingIntent mPendingIntent;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private CustomTabActivityTimeoutHandler mTimeoutHandler;

    private static final int TIMEOUT_MINUTES = 5;
    private static final Intent sIntentWithExtra = new Intent();

    @Before
    public void setUp() {
        ChromeFeatureList.sCctResetMinimumTimeoutMinutes.setForTesting(1);
        sIntentWithExtra.removeExtra(CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_PENDING_INTENT);
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES, TIMEOUT_MINUTES);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
    }

    @Test
    public void onResume_timeoutElapsed_finishesActivity() {
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED})
    public void onResume_flagDisabled_ignoresTimeout() {
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_timeoutNotElapsed_doesNotFinishActivity() {
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES - 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_noTimeoutSet_doesNotFinishActivity() {
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, new Intent());
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(); // No timeout set
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_userDidNotLeave_doesNotFinishActivity() {
        // onUserLeaveHint is not called.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_calledTwiceAfterSingleLeave_finishesActivityOnlyOnce() {
        // First timeout
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable).run();

        // Second onResume without another onUserLeaveHint should not trigger finish again.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        // Verify it was only called once.
        verify(mFinishRunnable, times(1)).run();
    }

    @Test
    public void onRestoreInstanceState_stateRestoredAfterLeave_finishesOnTimeout() {
        mTimeoutHandler.onUserLeaveHint();

        Bundle outState = new Bundle();
        mTimeoutHandler.onSaveInstanceState(outState);

        // Create a new handler to simulate activity recreation.
        CustomTabActivityTimeoutHandler newTimeoutHandler =
                new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
        newTimeoutHandler.restoreInstanceState(outState);

        // Time advances, and the timeout should trigger as the leave time was restored.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        newTimeoutHandler.onResume();
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_specifiedTimeoutLessThanMinimumTimeout_minimumTimeoutUsed() {
        int minimumTimeoutMinutes = 10;
        ChromeFeatureList.sCctResetMinimumTimeoutMinutes.setForTesting(minimumTimeoutMinutes);
        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable, never()).run();

        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(minimumTimeoutMinutes + 1));
        mTimeoutHandler.onResume();
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_timeoutElapsed_closingIntentAttached_sendsIntent() throws Exception {
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_PENDING_INTENT,
                mPendingIntent);

        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);

        mTimeoutHandler.onUserLeaveHint();
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume();
        verify(mPendingIntent).send();
        verify(mFinishRunnable, never()).run();
    }
}
