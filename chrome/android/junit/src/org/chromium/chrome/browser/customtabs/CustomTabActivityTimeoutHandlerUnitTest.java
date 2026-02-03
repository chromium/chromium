// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.PowerManager;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowPowerManager;

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
    private Context mContext;
    private ShadowPowerManager mShadowPowerManager;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        ChromeFeatureList.sCctResetMinimumTimeoutMinutes.setForTesting(1);
        ChromeFeatureList.sCctResetMinimumTimeoutMinutesAllowed.setForTesting(1);
        sIntentWithExtra.removeExtra(CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_PENDING_INTENT);
        sIntentWithExtra.removeExtra(CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES);
        sIntentWithExtra.removeExtra(CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES_ALLOWED);

        // Default setup for Chrome experiment. The class is annotated with @EnableFeatures for it.
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES_ALLOWED, TIMEOUT_MINUTES);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);

        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mShadowPowerManager = shadowOf(powerManager);
        mShadowPowerManager.setIsInteractive(true);
    }

    @Test
    public void onResume_timeoutElapsed_finishesActivity() {
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED})
    public void onResume_flagDisabled_ignoresTimeout() {
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_timeoutNotElapsed_doesNotFinishActivity() {
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES - 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_noTimeoutSet_doesNotFinishActivity() {
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, new Intent());
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext); // No timeout set
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_userDidNotLeave_doesNotFinishActivity() {
        // onStop is not called.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_calledTwiceAfterSingleLeave_finishesActivityOnlyOnce() {
        // First timeout
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();

        // Second onResume without another onStop should not trigger finish again.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext);
        // Verify it was only called once.
        verify(mFinishRunnable, times(1)).run();
    }

    @Test
    public void onRestoreInstanceState_stateRestoredAfterLeave_finishesOnTimeout() {
        mTimeoutHandler.onStop(mContext);

        Bundle outState = new Bundle();
        mTimeoutHandler.onSaveInstanceState(outState);

        // Create a new handler to simulate activity recreation.
        CustomTabActivityTimeoutHandler newTimeoutHandler =
                new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
        newTimeoutHandler.restoreInstanceState(outState);

        // Time advances, and the timeout should trigger as the leave time was restored.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        newTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_specifiedTimeoutLessThanMinimumTimeout_minimumTimeoutUsed() {
        int minimumTimeoutMinutes = 10;
        ChromeFeatureList.sCctResetMinimumTimeoutMinutes.setForTesting(minimumTimeoutMinutes);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();

        // Reset and try again with sufficient time.
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(minimumTimeoutMinutes + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_timeoutElapsed_closingIntentAttached_sendsIntent() throws Exception {
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_PENDING_INTENT, mPendingIntent);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mPendingIntent)
                .send(eq(mContext), eq(0), isNull(), isNull(), isNull(), isNull(), any());
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onStop_isLockingScreen_doesNotSetTimestamp() {
        mShadowPowerManager.setIsInteractive(false);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onStop_isLaunchingExternalActivity_doesNotSetTimestamp() {
        mTimeoutHandler.setLaunchingExternalActivity(true);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED)
    @EnableFeatures(ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED)
    public void onResume_timeoutElapsed_finishesActivity_embedderExperiment() {
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES, TIMEOUT_MINUTES);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED,
        ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED
    })
    public void onResume_flagDisabled_ignoresTimeout_embedderExperiment() {
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES, TIMEOUT_MINUTES);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED)
    @EnableFeatures(ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED)
    public void
            onResume_specifiedTimeoutLessThanMinimumTimeout_minimumTimeoutUsed_embedderExperiment() {
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES, TIMEOUT_MINUTES);

        int minimumTimeoutMinutes = 10;
        ChromeFeatureList.sCctResetMinimumTimeoutMinutesAllowed.setForTesting(
                minimumTimeoutMinutes);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();

        // Reset and try again with sufficient time.
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(minimumTimeoutMinutes + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_RESET_TIMEOUT_ENABLED,
        ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED
    })
    public void onResume_bothExtrasPresent_embedderExperimentUsed() {
        int chromeTimeout = 10;
        int embedderTimeout = 5;
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES_ALLOWED, chromeTimeout);
        sIntentWithExtra.putExtra(
                CustomTabActivityTimeoutHandler.EXTRA_TIMEOUT_MINUTES, embedderTimeout);
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(mFinishRunnable, sIntentWithExtra);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(embedderTimeout + 1));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }
}
