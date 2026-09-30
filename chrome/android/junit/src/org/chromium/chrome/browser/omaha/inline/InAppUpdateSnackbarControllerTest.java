// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.gms.ChromiumPlayServicesAvailability;

/** Unit tests for {@link InAppUpdateSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
public class InAppUpdateSnackbarControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Runnable mOnAccept;
    @Mock private Runnable mOnDismiss;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivity = mActivityController.get();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(true);
        lenient().when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(InAppUpdatePolicy.PREF_KEY_DISCOVERY_BACKOFF);
        ChromeSharedPreferences.getInstance().removeKey(InAppUpdatePolicy.PREF_KEY_RESTART_BACKOFF);
    }

    @Test
    public void testShowDiscovery_activityFinishing_returnsNull() {
        mActivity.finish();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowRestart_activityFinishing_returnsNull() {
        mActivity.finish();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowDiscovery_activityDestroyed_returnsNull() {
        mActivityController.destroy();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowRestart_activityDestroyed_returnsNull() {
        mActivityController.destroy();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowDiscovery_cannotShowSnackbar_returnsNull() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(false);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowRestart_cannotShowSnackbar_returnsNull() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(false);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT)
                        .build();

        assertNull(
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss));
        verify(mSnackbarManager, never()).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    public void testShowDiscovery_success_displaysSnackbarAndLogsImpression() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);

        assertNotNull(controller);
        watcher.assertExpected();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.in_app_update_discovery_title),
                snackbar.getTextForTesting());
        assertEquals(
                mActivity.getString(R.string.in_app_update_discovery_action),
                snackbar.getActionText());
        assertEquals(Snackbar.UMA_IN_APP_UPDATE_DISCOVERY, snackbar.getIdentifierForTesting());
        assertEquals(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS, snackbar.getDuration());
        assertEquals(controller, snackbar.getController());
    }

    @Test
    public void testShowRestart_success_displaysSnackbarAndLogsImpression() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        var controller =
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);

        assertNotNull(controller);
        watcher.assertExpected();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(
                mActivity.getString(R.string.in_app_update_restart_title),
                snackbar.getTextForTesting());
        assertEquals(
                mActivity.getString(R.string.in_app_update_restart_action),
                snackbar.getActionText());
        assertEquals(Snackbar.UMA_IN_APP_UPDATE_RESTART, snackbar.getIdentifierForTesting());
        assertEquals(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS, snackbar.getDuration());
        assertEquals(controller, snackbar.getController());
    }

    @Test
    public void testShowDiscovery_withoutOnDismiss_displaysSnackbarAndRecordsDecline() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(mActivity, mSnackbarManager, mOnAccept);

        assertNotNull(controller);
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnAccept, never()).run();
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    public void testShowRestart_withoutOnDismiss_displaysSnackbarAndRecordsDecline() {
        var controller =
                InAppUpdateSnackbarController.showRestart(mActivity, mSnackbarManager, mOnAccept);

        assertNotNull(controller);
        verify(mSnackbarManager).showSnackbar(any());
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnAccept, never()).run();
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    public void testShowDiscovery_onAction_executesCallbackAndLogsActionTapped() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.ACTION_TAPPED);

        controller.onAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnAccept).run();
        verify(mOnDismiss, never()).run();
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    public void testShowRestart_onAction_executesCallbackAndLogsActionTapped() {
        var controller =
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.ACTION_TAPPED);

        controller.onAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnAccept).run();
        verify(mOnDismiss, never()).run();
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    public void testShowDiscovery_onDismissNoAction_recordsThrottleRunsDismissAndLogsEvent() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    public void testShowRestart_onDismissNoAction_recordsThrottleRunsDismissAndLogsEvent() {
        var controller =
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    public void
            testOnDismissNoAction_activityStopped_recordsLifecycleDismissAndSuppressesBackoff() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        // Simulate activity transitioning to STOPPED (e.g. app backgrounded).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        // Teardown/backgrounding must not throttle future prompts.
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    public void
            testOnDismissNoAction_activityFinishing_recordsLifecycleDismissAndSuppressesBackoff() {
        var controller =
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        mActivity.finish();

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    public void
            testOnDismissNoAction_activityChangingConfigurations_recordsLifecycleDismissAndSuppressesBackoff() {
        Activity spyActivity = spy(mActivity);
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        spyActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(spyActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        // Simulate screen rotation / configuration change in progress.
        doReturn(true).when(spyActivity).isChangingConfigurations();

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(spyActivity));
    }

    @Test
    public void
            testOnDismissNoAction_activityDestroyed_recordsLifecycleDismissAndSuppressesBackoff() {
        var controller =
                InAppUpdateSnackbarController.showRestart(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.DESTROYED);

        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss).run();
        verify(mOnAccept, never()).run();
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    public void testDismiss_teardownSuppressesMetricsAndBackoff() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        // Teardown dismisses the snackbar.
        controller.dismiss();
        verify(mSnackbarManager).dismissSnackbars(controller);

        // Simulate SnackbarManager notifying controller of dismissal during teardown.
        controller.onDismissNoAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnDismiss, never()).run();
        verify(mOnAccept, never()).run();
        // Teardown dismissal must not throttle future prompts.
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    public void testDismiss_idempotence() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);

        controller.dismiss();
        verify(mSnackbarManager, times(1)).dismissSnackbars(controller);

        // Calling dismiss a second time is an idempotent no-op.
        controller.dismiss();
        verify(mSnackbarManager, times(1)).dismissSnackbars(controller);
    }

    @Test
    public void testOnAction_afterTeardownDismiss_isSuppressed() {
        var controller =
                InAppUpdateSnackbarController.showDiscovery(
                        mActivity, mSnackbarManager, mOnAccept, mOnDismiss);
        assertNotNull(controller);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        controller.dismiss();
        controller.onAction(/* actionData= */ null);

        watcher.assertExpected();
        verify(mOnAccept, never()).run();
    }
}
