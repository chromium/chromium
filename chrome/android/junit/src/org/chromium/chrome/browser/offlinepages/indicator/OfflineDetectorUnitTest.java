// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.offlinepages.indicator.OfflineDetector.STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS;
import static org.chromium.chrome.browser.offlinepages.indicator.OfflineDetector.STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS;
import static org.chromium.chrome.browser.offlinepages.indicator.OfflineDetector.setMockElapsedTimeSupplier;

import android.app.Application;
import android.content.ContentResolver;
import android.os.Handler;
import android.provider.Settings;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApplicationState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetector;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetector.ConnectionState;

/** Unit tests for {@link OfflineDetector}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OfflineDetectorUnitTest {
    @Mock private ConnectivityDetector mConnectivityDetector;
    @Mock private Handler mHandler;

    private long mElapsedTimeMs;
    private OfflineDetector mOfflineDetector;

    private int mIsOfflineNotificationsReceivedByObserver;
    private boolean mLastNotificationReceivedIsOffline;

    private int mIsForegroundNotificationsReceivedByObserver;
    private boolean mLastNotificationReceivedIsForeground;

    private ContentResolver mContentResolver;
    private Application mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mContentResolver = mContext.getContentResolver();

        mElapsedTimeMs = 0;
        OfflineDetector.setMockElapsedTimeSupplier(() -> mElapsedTimeMs);

        OfflineDetector.setMockConnectivityDetector(mConnectivityDetector);

        mOfflineDetector =
                new OfflineDetector(
                        (Boolean offline) -> onConnectionStateChanged(offline),
                        (Boolean isForeground) -> onApplicationStateChanged(isForeground),
                        mContext);
        mOfflineDetector.setHandlerForTesting(mHandler);
    }

    @After
    public void tearDown() {
        OfflineDetector.setMockElapsedTimeSupplier(null);
        Settings.System.putInt(mContentResolver, Settings.Global.AIRPLANE_MODE_ON, 0);
    }

    /**
     * Tests that the online notification is sent immediately when the device goes online.
     * Also, verifies that the offline notification is sent after
     * |STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS| time has elapsed.
     */
    @Test
    public void testCallbackInvokedOnConnectionChange() {
        // Set the app state to foreground before running the tests.
        changeApplicationStateToBackground(false);
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);

        // Change to online.
        changeConnectionState(false);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        // Change to offline.
        changeConnectionState(true);
        assertEquals(
                "Notification received immediately after connection changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to offline",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS);
        captor.getValue().run();

        assertEquals(
                "Notification count not updated after connection changed to offline",
                2,
                mIsOfflineNotificationsReceivedByObserver);
        assertTrue(
                "Notification not received after connection changed to offline",
                mLastNotificationReceivedIsOffline);

        // Change to online.
        changeConnectionState(false);
        assertEquals(
                "Notification count not updated after connection changed to online",
                3,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification not received after connection changed to online",
                mLastNotificationReceivedIsOffline);

        // Change to online again. It should not trigger a notification.
        changeConnectionState(false);
        assertEquals(
                "Extra notification received even though there is no change in connection state",
                3,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though there is no change in connection state",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Reports the device as offline while the app is in background. Then, after a long time, the
     * app is brought to foreground. The test verifies that the offline callback is invoked after
     * |STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS|.
     */
    @Test
    public void testCallbackNotInvokedOfflineBackgroundToForeground() {
        // Change to online.
        changeConnectionState(false);
        assertEquals(0, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        assertEquals(
                "Notification received immediately after connection changed to offline",
                0,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to offline.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Report the app as backgrounded and then report the device as offline. This is the
        // behavior experienced by apps that are prohibited from using data while in background.
        changeApplicationStateToBackground(true);
        changeConnectionState(true);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // Report the app state as foregrounded. The offline state should not be notified
        // immediately.
        changeApplicationStateToBackground(false);

        verify(mHandler)
                .postDelayed(captor.capture(), eq(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS));

        assertEquals(
                "Extra notification received even though app just returned to foreground",
                0,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though app just returned to foreground",
                mLastNotificationReceivedIsOffline);

        // Advance time after which the offline state should be notified.
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Expected notification when app has been in foreground for long",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertTrue(
                "Expected notification when app has been in foreground for long",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Reports the device as offline while the app is in background. Then, after a long time, the
     * app is brought to foreground. The test verifies that the offline callback is not invoked
     * immediately and that the callback is cancelled if the device state is reported as online in
     * the meantime.
     */
    @Test
    public void testCallbackNotInvokedOfflineBackgroundToForegroundBatterySaver() {
        // Change to online.
        changeConnectionState(false);
        assertEquals(0, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        assertEquals(
                "Notification received immediately after connection changed to offline",
                0,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to offline.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Report the app as backgrounded and then report the device as offline. This is the
        // behavior experienced by apps that are prohibited from using data while in background.
        changeApplicationStateToBackground(true);
        changeConnectionState(true);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // Bring the app to foreground. The offline status should not be notified immediately.
        changeApplicationStateToBackground(false);

        verify(mHandler)
                .postDelayed(captor.capture(), eq(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS));

        assertEquals(
                "Extra notification received even though connection is still effectively online",
                0,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Connection is reported as offline when it's online",
                mLastNotificationReceivedIsOffline);

        // Received online notification. This should cancel the pending offline callback.
        changeConnectionState(false);

        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Extra notification received even though connection is still online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Connection is reported as online when it's offline",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Reports the device as offline followed by online while the app is in background. Then, after
     * a long time, the app is brought to foreground. The test verifies that the online callback is
     * invoked immediately.
     */
    @Test
    public void testCallbackInvokedOnlineBackgroundToForeground() {
        // Set the app state to foreground before running the tests.
        changeApplicationStateToBackground(false);
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);

        // Change to online.
        changeConnectionState(false);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        assertEquals(
                "Duplicate notification received after connection changed to online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Duplicate notification received after connection changed to online.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Report the connection as offline and then app as backgrounded.
        changeConnectionState(true);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Notification not received even though connection is now offline",
                2,
                mIsOfflineNotificationsReceivedByObserver);
        assertTrue(
                "Notification not received even though connection is now offline",
                mLastNotificationReceivedIsOffline);

        // Advance time by a long duration (10 minutes) and report app as backgrounded.
        advanceTimeByMs(10 * 60 * 1000);
        changeApplicationStateToBackground(true);

        // Report the connection as online and move clock.
        changeConnectionState(false);
        advanceTimeByMs(10 * 60 * 1000);

        // Report the app state as foregrounded. The online state should be notified
        // immediately.
        changeApplicationStateToBackground(false);
        captor.getValue().run();
        assertEquals(
                "Notification not received even though connection is now online",
                3,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification not received even though connection is now online",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Tests that when the device switches immediately from offline to online, then the callback is
     * not executed.
     */
    @Test
    public void testCallbackNotInvokedOfflineToFastOnline() {
        // Set the app state to foreground before running the tests.
        changeApplicationStateToBackground(false);
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);

        // Change to online.
        changeConnectionState(false);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        // Change to offline.
        changeConnectionState(true);
        assertEquals(
                "Notification received immediately after connection changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to offline.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));
        advanceTimeByMs(
                STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS - 1000L);

        assertEquals(
                "Notification received soon after connection changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received soon after connection changed to offline",
                mLastNotificationReceivedIsOffline);

        // Change to online.
        changeConnectionState(false);
        assertEquals(
                "Extra notification received after connection changed to online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Connection is reported as offline when it's online",
                mLastNotificationReceivedIsOffline);

        // Move clock forward by 1000ms so that the offline callback posts after
        // |STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS|. This should not trigger a notification
        // since the connection is now online.
        advanceTimeByMs(1000L);
        captor.getValue().run();
        assertEquals(
                "Extra notification received even though connection is still online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Connection is reported as offline when it's online",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Waits |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| before
     * reporting the device as offline if the app has been on an online connection before.
     */
    @Test
    public void testCallbackNotInvokedOfflineInForeground() {
        changeApplicationStateToBackground(false);
        // Change to online.
        changeConnectionState(false);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        assertEquals(
                "Notification received immediately after connection changed to online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to online.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // The offline state should not be notified after
        // |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| since the device
        // has been in the foreground for a long time.
        changeConnectionState(true);

        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));

        assertEquals(
                "Extra notification received even though device just changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device just changed to offline",
                mLastNotificationReceivedIsOffline);

        // Advance time after which the offline state should be notified.
        advanceTimeByMs(
                STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS
                        - STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Expected notification when app has been offline for long",
                2,
                mIsOfflineNotificationsReceivedByObserver);
        assertTrue(
                "Expected notification when app has been offline for long",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Waits |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| before
     * reporting the device as offline if the app has been on an online connection before. If the
     * device is reported as online in the meantime, then the posted task is cancelled and the
     * callback is not invoked.
     */
    @Test
    public void testCallbackNotInvokedOfflineInForegroundCancelled() {
        changeApplicationStateToBackground(false);
        // Change to online.
        changeConnectionState(false);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        assertEquals(
                "Notification received immediately after connection changed to online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Notification received immediately after connection changed to online.",
                mLastNotificationReceivedIsOffline);
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // The offline state should not be notified after
        // |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| since the device
        // has been in the foreground for a long time.
        changeConnectionState(true);

        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));

        assertEquals(
                "Extra notification received even though device just changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device just changed to offline",
                mLastNotificationReceivedIsOffline);

        // Change to online before
        // |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| duration is over.
        // This should cancel the callbacks.
        changeConnectionState(false);

        // Advance time after which the offline state should be notified.
        advanceTimeByMs(
                STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS
                        - STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Extra notification received even though device is now online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device is now online",
                mLastNotificationReceivedIsOffline);
    }

    /**
     * Tests that the application state callback is called when the application switches between
     * background and foreground.
     */
    @Test
    public void testApplicationStateCallback() {
        changeApplicationStateToBackground(false);

        assertEquals(
                "Notification received when application state changes",
                1,
                mIsForegroundNotificationsReceivedByObserver);
        assertTrue(
                "Last notification received from application going to foreground",
                mLastNotificationReceivedIsForeground);
        assertTrue(
                "Stored state matches last notification",
                mOfflineDetector.isApplicationForeground());

        changeApplicationStateToBackground(false);

        assertEquals(
                "No notification received if application state doesn't change",
                1,
                mIsForegroundNotificationsReceivedByObserver);
        assertTrue(
                "Last notification received from application going to foreground",
                mLastNotificationReceivedIsForeground);
        assertTrue(
                "Stored state matches last notification",
                mOfflineDetector.isApplicationForeground());

        changeApplicationStateToBackground(true);

        assertEquals(
                "Notification received when application state changes",
                2,
                mIsForegroundNotificationsReceivedByObserver);
        assertFalse(
                "Last notification received from application going to background",
                mLastNotificationReceivedIsForeground);
        assertFalse(
                "Stored state matches last notification",
                mOfflineDetector.isApplicationForeground());

        changeApplicationStateToBackground(true);

        assertEquals(
                "No notification received if application state doesn't change",
                2,
                mIsForegroundNotificationsReceivedByObserver);
        assertFalse(
                "Last notification received from application going to background",
                mLastNotificationReceivedIsForeground);
        assertFalse(
                "Stored state matches last notification",
                mOfflineDetector.isApplicationForeground());

        changeApplicationStateToBackground(false);

        assertEquals(
                "Notification received when application state changes",
                3,
                mIsForegroundNotificationsReceivedByObserver);
        assertTrue(
                "Last notification received from application going to foreground",
                mLastNotificationReceivedIsForeground);
        assertTrue(
                "Stored state matches last notification",
                mOfflineDetector.isApplicationForeground());
    }

    @Test
    public void testAirplaneModeToOffline() {
        changeApplicationStateToBackground(false);

        // Simulate offline + airplane mode.
        Settings.System.putInt(mContentResolver, Settings.Global.AIRPLANE_MODE_ON, 1);
        changeConnectionState(true);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // Simulate airplane mode change to false, while still offline.
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        Settings.System.putInt(mContentResolver, Settings.Global.AIRPLANE_MODE_ON, 0);
        changeConnectionState(true);

        // Offline status shouldn't be communicated until
        // SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS elapses.
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));

        assertEquals(
                "Extra notification received even though device just changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device just changed to offline",
                mLastNotificationReceivedIsOffline);

        // Verify offline status is communicated if time elaspses.
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS);
        captor.getValue().run();

        assertEquals(
                "Notification count not updated after connection changed to offline",
                2,
                mIsOfflineNotificationsReceivedByObserver);
        assertTrue(
                "Notification not received after connection changed to offline",
                mLastNotificationReceivedIsOffline);
    }

    @Test
    public void testAirplaneModeToOnline() {
        changeApplicationStateToBackground(false);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);

        // Start online.
        changeConnectionState(true);
        assertEquals(1, mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        verify(mHandler)
                .postDelayed(captor.capture(), eq(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS));

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // Simulate offline + airplane mode.
        Settings.System.putInt(mContentResolver, Settings.Global.AIRPLANE_MODE_ON, 1);
        changeConnectionState(true);

        // #updateState will still run again since connection state has changed.
        verify(mHandler, times(2))
                .postDelayed(captor.capture(), eq(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS));

        // Advance time after which runnable will execute
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_OFFLINE_DURATION_MS);
        captor.getValue().run();

        // Effective offline status hasn't changed.
        assertEquals(
                "Effective offline status didn't change",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(mLastNotificationReceivedIsOffline);

        // Advance time by a long duration (10 minutes).
        advanceTimeByMs(10 * 60 * 1000);

        // Simulate airplane mode change to false, while still offline.
        Settings.System.putInt(mContentResolver, Settings.Global.AIRPLANE_MODE_ON, 0);
        changeConnectionState(true);

        // Offline status shouldn't be communicated until
        // SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS elapses.
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS));

        assertEquals(
                "Extra notification received even though device just changed to offline",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device just changed to offline",
                mLastNotificationReceivedIsOffline);

        // Change to online before
        // |STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS| duration is over.
        // This should cancel the callbacks.
        changeConnectionState(false);

        // Advance time after which the offline state should be notified.
        advanceTimeByMs(STATUS_INDICATOR_WAIT_ON_SWITCH_ONLINE_TO_OFFLINE_DEFAULT_DURATION_MS);
        captor.getValue().run();
        assertEquals(
                "Extra notification received even though device is now online",
                1,
                mIsOfflineNotificationsReceivedByObserver);
        assertFalse(
                "Extra notification received even though device is now online",
                mLastNotificationReceivedIsOffline);
    }

    private void changeConnectionState(boolean offline) {
        final int state = offline ? ConnectionState.NO_INTERNET : ConnectionState.VALIDATED;
        mOfflineDetector.onConnectionStateChanged(state);
    }

    private void changeApplicationStateToBackground(boolean changeToBackground) {
        mOfflineDetector.onApplicationStateChange(
                changeToBackground
                        ? ApplicationState.HAS_STOPPED_ACTIVITIES
                        : ApplicationState.HAS_RUNNING_ACTIVITIES);
    }

    private void advanceTimeByMs(long delta) {
        mElapsedTimeMs += delta;
        setMockElapsedTimeSupplier(() -> mElapsedTimeMs);
    }

    private void onConnectionStateChanged(boolean offline) {
        mIsOfflineNotificationsReceivedByObserver++;
        mLastNotificationReceivedIsOffline = offline;
    }

    private void onApplicationStateChanged(boolean isForeground) {
        mIsForegroundNotificationsReceivedByObserver++;
        mLastNotificationReceivedIsForeground = isForeground;
    }
}
