// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.os.Bundle;
import android.os.PowerManager;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsTimeoutOutcome.CustomTabsResetTimeoutOutcome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link CustomTabActivityTimeoutHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED,
    ChromeFeatureList.CCT_RESET_TIMEOUT_SKIP_CONFIGURATION_CHANGES
})
public class CustomTabActivityTimeoutHandlerUnitTest {
    @Mock private Runnable mFinishRunnable;
    @Mock private PendingIntent mPendingIntent;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private CustomTabActivityTimeoutHandler mTimeoutHandler;

    private static final int TIMEOUT_MINUTES = 5;
    private Activity mActivity;
    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private Context mContext;
    private ShadowPowerManager mShadowPowerManager;

    private static BrowserServicesIntentDataProvider createIntentDataProvider(
            @Nullable Integer timeoutMinutes, @Nullable PendingIntent pendingIntent) {
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        when(provider.getTimeoutMinutes()).thenReturn(timeoutMinutes);
        when(provider.getTimeoutPendingIntent()).thenReturn(pendingIntent);
        return provider;
    }

    private static long getSavedLeaveTimestamp(CustomTabActivityTimeoutHandler handler) {
        Bundle outState = new Bundle();
        handler.onSaveInstanceState(outState);
        return outState.getLong(CustomTabActivityTimeoutHandler.KEY_LEAVE_TIMESTAMP, -1L);
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mActivity = Robolectric.buildActivity(Activity.class).get();
        ChromeFeatureList.sCctResetMinimumTimeoutMinutesAllowed.setForTesting(1);
        ChromeFeatureList.sCctResetTimeoutMinutesOverride.setForTesting(0);
        mIntentDataProvider = createIntentDataProvider(TIMEOUT_MINUTES, null);

        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(mContext, mFinishRunnable, mIntentDataProvider);

        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        mShadowPowerManager = shadowOf(powerManager);
        mShadowPowerManager.turnScreenOn(true);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    public void onResume_timeoutElapsed_finishesActivity() {
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_ALLOWED})
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
        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        mContext, mFinishRunnable, createIntentDataProvider(null, null));
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
                new CustomTabActivityTimeoutHandler(mContext, mFinishRunnable, mIntentDataProvider);
        newTimeoutHandler.restoreInstanceState(outState);

        // Time advances, and the timeout should trigger as the leave time was restored.
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        newTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_specifiedTimeoutLessThanMinimumTimeout_minimumTimeoutUsed() {
        int minimumTimeoutMinutes = 10;
        ChromeFeatureList.sCctResetMinimumTimeoutMinutesAllowed.setForTesting(
                minimumTimeoutMinutes);
        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(mContext, mFinishRunnable, mIntentDataProvider);

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
    public void onResume_overrideTimeoutSet_overrideTimeoutUsed() {
        ChromeFeatureList.sCctResetTimeoutMinutesOverride.setForTesting(2);
        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(mContext, mFinishRunnable, mIntentDataProvider);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(3));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_timeoutElapsed_closingIntentAttached_sendsIntent() throws Exception {
        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        mContext,
                        mFinishRunnable,
                        createIntentDataProvider(TIMEOUT_MINUTES, mPendingIntent));
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mPendingIntent)
                .send(eq(mContext), eq(0), isNull(), isNull(), isNull(), isNull(), any());
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void timeoutConfiguredFromIntentDataProvider_withoutReadingIntent() throws Exception {
        BrowserServicesIntentDataProvider provider =
                createIntentDataProvider(TIMEOUT_MINUTES, mPendingIntent);

        CustomTabActivityTimeoutHandler timeoutHandler =
                new CustomTabActivityTimeoutHandler(mContext, mFinishRunnable, provider);
        timeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        timeoutHandler.onResume(mContext);

        verify(mPendingIntent)
                .send(eq(mContext), eq(0), isNull(), isNull(), isNull(), isNull(), any());
        verify(provider, never()).getIntent();
    }

    @Test
    public void onStop_isLockingScreen_doesNotSetTimestamp() {
        mShadowPowerManager.turnScreenOn(false);
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onStop_isLockingScreen_preservesExistingLeaveTimestamp() {
        mTimeoutHandler.onStop(mContext);
        long initialTimestamp = getSavedLeaveTimestamp(mTimeoutHandler);
        assertNotEquals(-1L, initialTimestamp);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));
        mShadowPowerManager.turnScreenOn(false);
        mTimeoutHandler.onStop(mContext);

        assertEquals(initialTimestamp, getSavedLeaveTimestamp(mTimeoutHandler));

        mShadowPowerManager.turnScreenOn(true);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
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
    public void onStop_isLaunchingExternalActivity_preservesExistingLeaveTimestamp() {
        mTimeoutHandler.onStop(mContext);
        long initialTimestamp = getSavedLeaveTimestamp(mTimeoutHandler);
        assertNotEquals(-1L, initialTimestamp);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));
        mTimeoutHandler.setLaunchingExternalActivity(true);
        mTimeoutHandler.onStop(mContext);

        assertEquals(initialTimestamp, getSavedLeaveTimestamp(mTimeoutHandler));

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES));
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_timeoutNotElapsed_recordsReturnedBeforeTimeout() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.RETURNED_BEFORE_TIMEOUT)
                        .build();
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES - 1));

        mTimeoutHandler.onResume(mContext);

        histogramWatcher.assertExpected();
    }

    @Test
    public void onResume_timeoutElapsed_recordsResetTriggeredFallback() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_FALLBACK)
                        .build();
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);

        histogramWatcher.assertExpected();
    }

    @Test
    public void onResume_timeoutElapsed_withIntent_recordsResetTriggeredIntent() throws Exception {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.RESET_TRIGGERED_INTENT)
                        .build();
        mTimeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        mContext,
                        mFinishRunnable,
                        createIntentDataProvider(TIMEOUT_MINUTES, mPendingIntent));
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);

        histogramWatcher.assertExpected();
    }

    @Test
    public void destroy_recordsSessionClosedManually() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.SESSION_CLOSED_MANUALLY)
                        .build();
        assertFalse(mActivity.isChangingConfigurations());
        mTimeoutHandler.onDestroy(mActivity);

        histogramWatcher.assertExpected();
    }

    @Test
    public void destroy_isChangingConfigurations_skipsRecordingSessionClosedManually() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(CustomTabsTimeoutOutcome.HISTOGRAM_NAME)
                        .build();
        Activity activity = createActivityChangingConfigurations();
        mTimeoutHandler.onDestroy(activity);

        histogramWatcher.assertExpected();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_SKIP_CONFIGURATION_CHANGES})
    public void destroy_isChangingConfigurations_flagDisabled_recordsSessionClosedManually() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.SESSION_CLOSED_MANUALLY)
                        .build();
        Activity activity = createActivityChangingConfigurations();
        mTimeoutHandler.onDestroy(activity);

        histogramWatcher.assertExpected();
    }

    @Test
    public void onResume_outcomeRecordedOnlyOnce() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                CustomTabsTimeoutOutcome.HISTOGRAM_NAME,
                                CustomTabsResetTimeoutOutcome.RETURNED_BEFORE_TIMEOUT)
                        .build();
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES - 1));
        mTimeoutHandler.onResume(mContext); // Records RETURNED_BEFORE_TIMEOUT

        // Call onResume again, should not record again.
        mTimeoutHandler.onResume(mContext);
        histogramWatcher.assertExpected();
    }

    @Test
    public void onResume_recordsElapsedTime() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("CustomTabs.ResetTimeout.ElapsedTimeInMinutesOnReturn")
                        .build();
        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES - 1));
        mTimeoutHandler.onResume(mContext);

        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_EARLY_INIT_POWER_MANAGER})
    public void onStop_earlyInitPowerManagerEnabled_usesConstructorPowerManager() {
        Context constructorContext = mock(Context.class);
        Context onStopContext = mock(Context.class);
        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        when(constructorContext.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);

        CustomTabActivityTimeoutHandler timeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        constructorContext, mFinishRunnable, mIntentDataProvider);
        verify(constructorContext).getSystemService(Context.POWER_SERVICE);

        mShadowPowerManager.turnScreenOn(true);
        timeoutHandler.onStop(onStopContext);
        verify(onStopContext, never()).getSystemService(Context.POWER_SERVICE);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        timeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_EARLY_INIT_POWER_MANAGER})
    public void onStop_earlyInitPowerManagerEnabled_lockingScreen_doesNotSetTimestamp() {
        Context constructorContext = mock(Context.class);
        Context onStopContext = mock(Context.class);
        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        when(constructorContext.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);

        CustomTabActivityTimeoutHandler timeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        constructorContext, mFinishRunnable, mIntentDataProvider);
        verify(constructorContext).getSystemService(Context.POWER_SERVICE);

        mShadowPowerManager.turnScreenOn(false);
        timeoutHandler.onStop(onStopContext);
        verify(onStopContext, never()).getSystemService(Context.POWER_SERVICE);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        timeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_EARLY_INIT_POWER_MANAGER})
    public void onStop_earlyInitPowerManagerDisabled_usesOnStopPowerManager() {
        Context constructorContext = mock(Context.class);
        Context onStopContext = mock(Context.class);
        PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
        when(onStopContext.getSystemService(Context.POWER_SERVICE)).thenReturn(powerManager);

        CustomTabActivityTimeoutHandler timeoutHandler =
                new CustomTabActivityTimeoutHandler(
                        constructorContext, mFinishRunnable, mIntentDataProvider);
        verify(constructorContext, never()).getSystemService(Context.POWER_SERVICE);

        mShadowPowerManager.turnScreenOn(true);
        timeoutHandler.onStop(onStopContext);
        verify(onStopContext).getSystemService(Context.POWER_SERVICE);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        timeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.CCT_EARLY_INIT_POWER_MANAGER})
    public void constructor_earlyInitPowerManagerEnabled_timeoutDisabled_doesNotGetPowerManager() {
        Context constructorContext = mock(Context.class);

        new CustomTabActivityTimeoutHandler(
                constructorContext, mFinishRunnable, createIntentDataProvider(null, null));
        verify(constructorContext, never()).getSystemService(Context.POWER_SERVICE);
    }

    /** Returns an Activity instance that was torn down due to a configuration change. */
    private static Activity createActivityChangingConfigurations() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        Activity activity = controller.get();
        // recreate() marks the original instance as changing configurations before stopping it.
        controller.recreate();
        assertTrue(activity.isChangingConfigurations());
        return activity;
    }

    @Test
    public void onStop_isChangingConfigurations_doesNotSetTimestamp() {
        Activity activity = createActivityChangingConfigurations();
        mTimeoutHandler.onStop(activity);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(activity);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onStop_isChangingConfigurations_preservesExistingLeaveTimestamp() {
        mTimeoutHandler.onStop(mActivity);
        long initialTimestamp = getSavedLeaveTimestamp(mTimeoutHandler);
        assertNotEquals(-1L, initialTimestamp);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(1));
        Activity changingActivity = createActivityChangingConfigurations();
        mTimeoutHandler.onStop(changingActivity);

        assertEquals(initialTimestamp, getSavedLeaveTimestamp(mTimeoutHandler));

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES));
        mTimeoutHandler.onResume(mActivity);
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_SKIP_CONFIGURATION_CHANGES})
    public void onStop_isChangingConfigurations_flagDisabled_setsTimestamp() {
        Activity activity = createActivityChangingConfigurations();
        mTimeoutHandler.onStop(activity);

        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));
        mTimeoutHandler.onResume(activity);
        verify(mFinishRunnable).run();
    }

    @Test
    public void onResume_applicationInBackground_skipsTimeoutEvaluation() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();
    }

    @Test
    public void onResume_applicationInBackground_evaluatesTimeoutOnSubsequentForegroundResume() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        // Background resume skips timeout
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable, never()).run();

        // Foreground resume evaluates timeout
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.CCT_RESET_TIMEOUT_SKIP_CONFIGURATION_CHANGES})
    public void onResume_applicationInBackground_flagDisabled_evaluatesTimeout() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        mTimeoutHandler.onStop(mContext);
        mFakeTimeTestRule.advanceMillis(TimeUnit.MINUTES.toMillis(TIMEOUT_MINUTES + 1));

        mTimeoutHandler.onResume(mContext);
        verify(mFinishRunnable).run();
    }
}
