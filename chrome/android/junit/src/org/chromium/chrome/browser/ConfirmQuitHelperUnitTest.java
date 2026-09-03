// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ConfirmQuitHelper.ConfirmQuitResult;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;

import java.time.Duration;

/** Unit tests for {@link ConfirmQuitHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ConfirmQuitHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ApplicationLifetime.Observer mObserver;

    private Context mContext;
    private ConfirmQuitHelper mConfirmQuitHelper;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mConfirmQuitHelper = ConfirmQuitHelper.getInstance();
        ApplicationLifetime.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        ConfirmQuitHelper.setInstanceForTesting(null);
    }

    @Test
    public void testGetInstance_ReturnsSingletonInstance() {
        ConfirmQuitHelper instance1 = ConfirmQuitHelper.getInstance();
        ConfirmQuitHelper instance2 = ConfirmQuitHelper.getInstance();

        assertNotNull(instance1);
        assertSame(instance1, instance2);
    }

    @Test
    public void testQuitRequest_TriggersToastAndHoldState() {
        var watcher =
                HistogramWatcher.newBuilder().expectNoRecords("Android.ConfirmQuit.Result").build();

        assertTrue(mConfirmQuitHelper.handleQuitRequest(mContext));
        assertTrue(mConfirmQuitHelper.isQuitInProgress());
        assertNotNull(mConfirmQuitHelper.getToastForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testQuitRequest_RepeatWhileHolding_Ignored() {
        assertTrue(mConfirmQuitHelper.handleQuitRequest(mContext));
        assertTrue(mConfirmQuitHelper.isQuitInProgress());

        // Advance past double-tap threshold but before hold duration.
        ShadowSystemClock.advanceBy(Duration.ofMillis(ConfirmQuitHelper.DOUBLE_TAP_DELTA_MS + 50));

        // Repeat quit request while still holding should be ignored without early termination.
        assertTrue(mConfirmQuitHelper.handleQuitRequest(mContext));
        assertTrue(mConfirmQuitHelper.isQuitInProgress());
        verify(mObserver, never()).onTerminate(false);

        // Advance the remainder of the hold duration to verify quit still succeeds.
        ShadowLooper.idleMainLooper(
                ConfirmQuitHelper.HOLD_DURATION_MS - (ConfirmQuitHelper.DOUBLE_TAP_DELTA_MS + 50));
        verify(mObserver).onTerminate(false);
    }

    @Test
    public void testDoubleTap_TriggersTermination() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.QUIT_DOUBLE_TAP);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        // Key-up between taps cancels the hold timer without prematurely recording CANCELED.
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);
        mConfirmQuitHelper.handleQuitRequest(mContext);

        verify(mObserver).onTerminate(false);
        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        assertNull(mConfirmQuitHelper.getToastForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testDoubleTap_ExceedsDelta_DoesNotTerminateImmediately() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.CANCELED);

        // First attempt initiated and key released.
        mConfirmQuitHelper.handleQuitRequest(mContext);
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);

        // Advance time past the double-tap delta threshold to fire the delayed CANCELED metric.
        ShadowSystemClock.advanceBy(Duration.ofMillis(ConfirmQuitHelper.DOUBLE_TAP_DELTA_MS + 50));
        ShadowLooper.idleMainLooper();
        watcher.assertExpected();

        var secondWatcher =
                HistogramWatcher.newBuilder().expectNoRecords("Android.ConfirmQuit.Result").build();

        // Second attempt after delta should start a new hold request rather than immediately
        // quitting.
        assertTrue(mConfirmQuitHelper.handleQuitRequest(mContext));
        assertTrue(mConfirmQuitHelper.isQuitInProgress());
        verify(mObserver, never()).onTerminate(false);
        secondWatcher.assertExpected();
    }

    @Test
    public void testHoldTimer_TriggersTermination() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.QUIT_HOLD);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        ShadowLooper.idleMainLooper(ConfirmQuitHelper.HOLD_DURATION_MS);

        verify(mObserver).onTerminate(false);
        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        assertNull(mConfirmQuitHelper.getToastForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testCancel_KeepsToastWhenDismissToastFalse() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.CANCELED);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);

        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        assertNotNull(mConfirmQuitHelper.getToastForTesting());

        // CANCELED is recorded after the double-tap window expires.
        ShadowSystemClock.advanceBy(Duration.ofMillis(ConfirmQuitHelper.DOUBLE_TAP_DELTA_MS + 50));
        ShadowLooper.idleMainLooper();
        watcher.assertExpected();
    }

    @Test
    public void testCancel_DismissesHoldStateAndToastWhenDismissToastTrue() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.CANCELED);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        mConfirmQuitHelper.cancel(/* dismissToast= */ true);

        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        assertNull(mConfirmQuitHelper.getToastForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testCancel_DismissToastTrueWhileDelayedRecordPending_RecordsCanceledImmediately() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.CANCELED);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        // Key-up schedules delayed CANCELED.
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);
        // Activity paused before double-tap window expires flushes CANCELED immediately.
        mConfirmQuitHelper.cancel(/* dismissToast= */ true);

        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        assertNull(mConfirmQuitHelper.getToastForTesting());
        watcher.assertExpected();
    }

    @Test
    public void testCancel_AfterDoubleTapDelta_RecordsCanceledImmediately() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ConfirmQuit.Result", ConfirmQuitResult.CANCELED);

        mConfirmQuitHelper.handleQuitRequest(mContext);
        ShadowSystemClock.advanceBy(Duration.ofMillis(ConfirmQuitHelper.DOUBLE_TAP_DELTA_MS + 50));

        // Releasing after double-tap delta cannot be a double-tap, so CANCELED records immediately.
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);
        watcher.assertExpected();
    }

    @Test
    public void testCancel_WhenNotInProgress_DoesNotRecordMetric() {
        var watcher =
                HistogramWatcher.newBuilder().expectNoRecords("Android.ConfirmQuit.Result").build();

        assertFalse(mConfirmQuitHelper.isQuitInProgress());
        mConfirmQuitHelper.cancel(/* dismissToast= */ false);
        mConfirmQuitHelper.cancel(/* dismissToast= */ true);

        watcher.assertExpected();
    }
}
