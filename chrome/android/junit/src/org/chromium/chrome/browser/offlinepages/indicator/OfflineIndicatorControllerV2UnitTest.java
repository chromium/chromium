// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2.STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS;
import static org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2.STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS;
import static org.chromium.chrome.browser.offlinepages.indicator.OfflineIndicatorControllerV2.setMockElapsedTimeSupplier;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetector;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetector.ConnectionState;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;

/** Unit tests for {@link OfflineIndicatorControllerV2}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OfflineIndicatorControllerV2UnitTest {
    @Mock private StatusIndicatorCoordinator mStatusIndicator;
    @Mock private ConnectivityDetector mConnectivityDetector;
    @Mock private OfflineDetector mOfflineDetector;
    @Mock private Handler mHandler;
    @Mock private Supplier<Boolean> mCanAnimateNativeBrowserControls;
    @Mock private OfflineIndicatorMetricsDelegate mMetricsDelegate;

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mIsUrlBarFocusedSupplier =
            new ObservableSupplierImpl<>();
    private OfflineIndicatorControllerV2 mController;
    private long mElapsedTimeMs;
    private String mOfflineString;
    private String mOnlineString;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(org.chromium.chrome.tab_ui.R.style.Theme_BrowserUI_DayNight);

        mOfflineString = mContext.getString(R.string.offline_indicator_v2_offline_text);
        mOnlineString = mContext.getString(R.string.offline_indicator_v2_back_online_text);

        when(mCanAnimateNativeBrowserControls.get()).thenReturn(true);
        when(mOfflineDetector.isApplicationForeground()).thenReturn(true);
        when(mMetricsDelegate.isTrackingShownDuration()).thenReturn(false);

        mIsUrlBarFocusedSupplier.set(false);
        OfflineDetector.setMockConnectivityDetector(mConnectivityDetector);
        OfflineIndicatorControllerV2.setMockOfflineDetector(mOfflineDetector);
        mElapsedTimeMs = 0;
        OfflineIndicatorControllerV2.setMockElapsedTimeSupplier(() -> mElapsedTimeMs);
        OfflineIndicatorControllerV2.setMockOfflineIndicatorMetricsDelegate(mMetricsDelegate);
        mController =
                new OfflineIndicatorControllerV2(
                        mContext,
                        mStatusIndicator,
                        mIsUrlBarFocusedSupplier,
                        mCanAnimateNativeBrowserControls);
        mController.setHandlerForTesting(mHandler);
    }

    @After
    public void tearDown() {
        OfflineIndicatorControllerV2.setMockElapsedTimeSupplier(null);
    }

    /** Tests that the offline indicator shows when the device goes offline. */
    @Test
    public void testShowsStatusIndicatorWhenOffline() {
        // Show.
        changeConnectionState(true);
        verify(mStatusIndicator).show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /** Tests that the offline indicator hides when the device goes online. */
    @Test
    public void testHidesStatusIndicatorWhenOnline() {
        // First, show.
        changeConnectionState(true);
        // Fast forward the cool-down.
        advanceTimeByMs(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS);
        // Now, hide.
        changeConnectionState(false);
        // When hiding, the indicator will get an #updateContent() call, then #hide() 2 seconds
        // after that. First, verify the #updateContent() call.
        final ArgumentCaptor<Runnable> endAnimationCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mStatusIndicator)
                .updateContent(
                        eq(mOnlineString),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        endAnimationCaptor.capture());
        // Simulate browser controls animation ending.
        endAnimationCaptor.getValue().run();
        // This should post a runnable to hide w/ a delay.
        final ArgumentCaptor<Runnable> hideCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        hideCaptor.capture(), eq(STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS));
        // Let's see if the Runnable we captured actually hides the indicator.
        hideCaptor.getValue().run();
        verify(mStatusIndicator).hide();
    }

    /** Tests that the indicator doesn't hide before the cool-down is complete. */
    @Test
    public void testCoolDown_Hide() {
        // First, show.
        changeConnectionState(true);
        // Advance time.
        advanceTimeByMs(3000);
        // Now, try to hide.
        changeConnectionState(false);

        // Cool-down should prevent it from hiding and post a runnable for after the time is up.
        verify(mStatusIndicator, never())
                .updateContent(any(), any(), anyInt(), anyInt(), anyInt(), any(Runnable.class));
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - 3000L));

        // Advance the time and simulate the |Handler| running the posted runnable.
        advanceTimeByMs(2000);
        captor.getValue().run();
        // #updateContent() should be called since the cool-down is complete.
        verify(mStatusIndicator)
                .updateContent(
                        eq(mOnlineString),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        any(Runnable.class));
    }

    /** Tests that the indicator doesn't show before the cool-down is complete. */
    @Test
    public void testCoolDown_Show() {
        // First, show.
        changeConnectionState(true);
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
        // Advance time so we can hide.
        advanceTimeByMs(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS);
        // Now, hide.
        changeConnectionState(false);

        // Try to show again, but before the cool-down is completed.
        advanceTimeByMs(1000);
        changeConnectionState(true);
        // Cool-down should prevent it from showing and post a runnable for after the time is up.
        // times(1) because it's been already called once above, no new calls.
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - 1000L));

        // Advance the time and simulate the |Handler| running the posted runnable.
        advanceTimeByMs(4000);
        captor.getValue().run();
        // #show() should be called since the cool-down is complete.
        verify(mStatusIndicator, times(2))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Tests that the indicator doesn't show if the device went back online after the show was
     * scheduled.
     */
    @Test
    public void testCoolDown_ChangeConnectionAfterShowScheduled() {
        changeConnectionState(true);
        advanceTimeByMs(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS);
        changeConnectionState(false);

        // Try to show, but before the cool-down is completed.
        advanceTimeByMs(1000);
        changeConnectionState(true);
        // Cool-down should prevent it from showing and post a runnable for after the time is up.
        // times(1) because it's been already called once above, no new calls.
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - 1000L));
        // Callbacks to show/hide are removed every time the connectivity changes. We use this to
        // capture the callback.
        verify(mHandler, times(3)).removeCallbacks(captor.getValue());
        // Advance time and change connection.
        advanceTimeByMs(2000);
        changeConnectionState(false);

        // Since we're back online, the posted runnable won't show the indicator.
        advanceTimeByMs(2000);
        captor.getValue().run();
        // Still times(1), no new call after the last one.
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /** Tests that the indicator doesn't show until the omnibox is unfocused. */
    @Test
    public void testOmniboxFocus_DelayShowing() {
        // Simulate focusing the omnibox.
        mIsUrlBarFocusedSupplier.set(true);
        // Now show, at least try.
        changeConnectionState(true);
        // Shouldn't show because the omnibox is focused.
        verify(mStatusIndicator, never())
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());

        // Should show once unfocused.
        mIsUrlBarFocusedSupplier.set(false);
        verify(mStatusIndicator).show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Tests that the indicator doesn't show when the omnibox is unfocused if the device goes back
     * online before the omnibox is unfocused.
     */
    @Test
    public void testOmniboxFocus_ChangeConnectionAfterShowScheduled() {
        // Simulate focusing the omnibox.
        mIsUrlBarFocusedSupplier.set(true);
        // Now show, at least try.
        changeConnectionState(true);
        // Shouldn't show because the omnibox is focused.
        verify(mStatusIndicator, never())
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());

        // Now, simulate going back online.
        changeConnectionState(false);
        // Unfocusing shouldn't cause a show because we're not offline.
        mIsUrlBarFocusedSupplier.set(false);
        verify(mStatusIndicator, never())
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Tests that the indicator waits for the omnibox to be unfocused if the omnibox was focused
     * when the cool-down ended and the indicator was going to be shown.
     */
    @Test
    public void testOmniboxIsFocusedWhenShownAfterCoolDown() {
        changeConnectionState(true);
        advanceTimeByMs(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS);
        changeConnectionState(false);

        // Try to show, but before the cool-down is completed.
        advanceTimeByMs(1000);
        changeConnectionState(true);
        // times(1) because it's been already called once above, no new calls.
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
        final ArgumentCaptor<Runnable> captor = ArgumentCaptor.forClass(Runnable.class);
        verify(mHandler)
                .postDelayed(
                        captor.capture(),
                        eq(STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - 1000L));

        // Now, simulate focusing the omnibox.
        mIsUrlBarFocusedSupplier.set(true);
        // Then advance the time and run the runnable.
        advanceTimeByMs(4000);
        captor.getValue().run();
        // Still times(1), no new calls. The indicator shouldn't show since the omnibox is focused.
        verify(mStatusIndicator, times(1))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
        // Should show once unfocused.
        mIsUrlBarFocusedSupplier.set(false);
        verify(mStatusIndicator, times(2))
                .show(eq(mOfflineString), any(), anyInt(), anyInt(), anyInt());
    }

    /**
     * Tests that we send the correct notifications to the metrics delegate when the connectivity
     * state changes between online and offline.
     */
    @Test
    public void testMetricsNotifications_ConnectionChange() {
        // Ensure that we don't update the metrics delegate on start up.
        verify(mMetricsDelegate, times(0)).onIndicatorShown();
        verify(mMetricsDelegate, times(0)).onIndicatorHidden();

        // When we go offline, make sure that we update the metrics delegate.
        changeConnectionState(true);
        verify(mMetricsDelegate, times(1)).onIndicatorShown();

        // Check that we don't update the metrics delegate if we remain offline.
        changeConnectionState(true);
        verify(mMetricsDelegate, times(1)).onIndicatorShown();

        // We advance the time to avoid the cool-down.
        advanceTimeByMs(5000);

        // Check that we update the metrics delegate when go back online.
        changeConnectionState(false);
        verify(mMetricsDelegate, times(1)).onIndicatorHidden();

        // Check that we don't update the metrics delegate if we remain online.
        changeConnectionState(false);
        verify(mMetricsDelegate, times(1)).onIndicatorHidden();

        advanceTimeByMs(5000);

        // When we go offline, make sure that we update the metrics delegate.
        changeConnectionState(true);
        verify(mMetricsDelegate, times(2)).onIndicatorShown();
    }

    /**
     * Tests that we send the correct notifications to the metrics delegate when the application
     * state changes between foreground and background.
     */
    @Test
    public void testMetricsNotifications_ApplicationStateChange() {
        // The Controller will inform the metrics delegate of the application state (which we have
        // defined as foreground in this case) when it is constructed.
        verify(mMetricsDelegate, times(1)).onAppForegrounded();
        verify(mMetricsDelegate, times(0)).onAppBackgrounded();

        // Check that we send a notification if the application state changes to background.
        changeApplicationState(false);
        verify(mMetricsDelegate, times(1)).onAppBackgrounded();

        // Check that we don't send a notification if the application state remains the same.
        changeApplicationState(false);
        verify(mMetricsDelegate, times(1)).onAppBackgrounded();

        // Check that we send a notification if the application state changes to foreground.
        changeApplicationState(true);
        verify(mMetricsDelegate, times(2)).onAppForegrounded();

        // Check that we don't send a notification if the application state remains the same.
        changeApplicationState(true);
        verify(mMetricsDelegate, times(2)).onAppForegrounded();

        // Check that we send a notification if the application state changes to background.
        changeApplicationState(false);
        verify(mMetricsDelegate, times(2)).onAppBackgrounded();
    }

    /**
     * Tests that we send the correct notifications to the metrics delegate when the application is
     * started while offline.
     */
    @Test
    public void testMetricsNotifications_StartUpOffline() {
        verify(mMetricsDelegate, times(0)).onIndicatorShown();
        verify(mMetricsDelegate, times(0)).onIndicatorHidden();
        verify(mMetricsDelegate, times(1)).onAppForegrounded();
        verify(mMetricsDelegate, times(0)).onAppBackgrounded();
        verify(mMetricsDelegate, times(0)).onOfflineStateInitialized(true);
        verify(mMetricsDelegate, times(0)).onOfflineStateInitialized(false);

        // Simulate the system going offline.
        changeConnectionState(true);
        verify(mMetricsDelegate, times(1)).onOfflineStateInitialized(true);
        verify(mMetricsDelegate, times(1)).onIndicatorShown();

        // Have the metrics delegate start tracking a shown duration.
        when(mMetricsDelegate.isTrackingShownDuration()).thenReturn(true);

        // Simulate the app being backgrounded.
        changeApplicationState(false);
        verify(mMetricsDelegate, times(1)).onAppBackgrounded();

        // Simulate the app being killed.
        mController = null;

        // Simulate the app being restarted, and still being offline.
        changeApplicationState(true);
        mController =
                new OfflineIndicatorControllerV2(
                        mContext,
                        mStatusIndicator,
                        mIsUrlBarFocusedSupplier,
                        mCanAnimateNativeBrowserControls);
        mController.setHandlerForTesting(mHandler);
        verify(mMetricsDelegate, times(2)).onAppForegrounded();

        // Simualte that we are still offline when the application is restarted,
        changeConnectionState(true);
        verify(mMetricsDelegate, times(2)).onOfflineStateInitialized(true);
        verify(mMetricsDelegate, times(2)).onIndicatorShown();

        advanceTimeByMs(5000);

        // Simulate the system coming back online.
        changeConnectionState(false);
        verify(mMetricsDelegate, times(1)).onIndicatorHidden();

        // Have the metrics delegate stop tracking a shown duration.
        when(mMetricsDelegate.isTrackingShownDuration()).thenReturn(false);
    }

    /**
     * Tests that we send the correct notifications to the metrics delegate when the application is
     * started while offline.
     */
    @Test
    public void testMetricsNotifications_StartUpOnline() {
        verify(mMetricsDelegate, times(0)).onIndicatorShown();
        verify(mMetricsDelegate, times(0)).onIndicatorHidden();
        verify(mMetricsDelegate, times(1)).onAppForegrounded();
        verify(mMetricsDelegate, times(0)).onAppBackgrounded();
        verify(mMetricsDelegate, times(0)).onOfflineStateInitialized(true);
        verify(mMetricsDelegate, times(0)).onOfflineStateInitialized(false);

        // Simulate the system going offline.
        changeConnectionState(true);
        // advanceTimeByMs(5000);
        verify(mMetricsDelegate, times(1)).onOfflineStateInitialized(true);
        verify(mMetricsDelegate, times(1)).onIndicatorShown();

        // Have the metrics delegate start tracking a shown duration.
        when(mMetricsDelegate.isTrackingShownDuration()).thenReturn(true);

        // Simulate the app being backgrounded.
        changeApplicationState(false);
        verify(mMetricsDelegate, times(1)).onAppBackgrounded();

        // Simulate the app being killed.
        mController = null;

        // Simulate the app being restarted, but now being online.
        changeApplicationState(true);
        mController =
                new OfflineIndicatorControllerV2(
                        mContext,
                        mStatusIndicator,
                        mIsUrlBarFocusedSupplier,
                        mCanAnimateNativeBrowserControls);
        mController.setHandlerForTesting(mHandler);
        verify(mMetricsDelegate, times(2)).onAppForegrounded();

        // If the system starts up online, we will get the signal immediately after the controller
        // is constructed.
        changeConnectionState(false);
        verify(mMetricsDelegate, times(1)).onOfflineStateInitialized(false);

        // Have the metrics delegate stop tracking a shown duration.
        when(mMetricsDelegate.isTrackingShownDuration()).thenReturn(false);
    }

    private void changeConnectionState(boolean offline) {
        final int state = offline ? ConnectionState.NO_INTERNET : ConnectionState.VALIDATED;
        when(mOfflineDetector.isConnectionStateOffline()).thenReturn(offline);
        mController.onConnectionStateChanged(offline);
    }

    private void changeApplicationState(boolean isForeground) {
        when(mOfflineDetector.isApplicationForeground()).thenReturn(isForeground);
        if (mController != null) {
            mController.onApplicationStateChanged(isForeground);
        }
    }

    private void advanceTimeByMs(long delta) {
        mElapsedTimeMs += delta;
        setMockElapsedTimeSupplier(() -> mElapsedTimeMs);
    }
}
