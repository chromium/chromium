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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.IntentSender;

import com.google.android.gms.tasks.Tasks;
import com.google.android.play.core.appupdate.AppUpdateInfo;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.appupdate.AppUpdateOptions;
import com.google.android.play.core.appupdate.testing.FakeAppUpdateManager;
import com.google.android.play.core.install.model.ActivityResult;
import com.google.android.play.core.install.model.AppUpdateType;
import com.google.android.play.core.install.model.InstallStatus;
import com.google.android.play.core.install.model.UpdateAvailability;

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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.gms.ChromiumPlayServicesAvailability;

/**
 * Sociable module-level tests for the in-app update flow, testing {@link InAppUpdateController},
 * {@link InAppUpdateSnackbarController}, and {@link InAppUpdatePolicy} interactions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InAppUpdateControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManager mSnackbarManager;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FakeAppUpdateManager mFakeAppUpdateManager;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        mActivity = mActivityController.get();
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.RESUMED);
        ChromiumPlayServicesAvailability.setIsAvailableForTesting(true);
        InAppUpdatePolicy.setIsOfficialBuildForTesting(true);
        lenient().when(mSnackbarManager.canShowSnackbar()).thenReturn(true);

        mFakeAppUpdateManager = new FakeAppUpdateManager(mActivity);
        InAppUpdateController.setAppUpdateManagerForTesting(mFakeAppUpdateManager);
    }

    @After
    public void tearDown() {
        InAppUpdateController.setAppUpdateManagerForTesting(null);
        InAppUpdatePolicy.clearBackoffsForTesting();
    }

    private InAppUpdateController createController() {
        return createController(mActivity);
    }

    private InAppUpdateController createController(Activity activity) {
        return new InAppUpdateController(activity, () -> mSnackbarManager);
    }

    private void setupUpdateAvailable() {
        mFakeAppUpdateManager.setUpdateAvailable(999999, AppUpdateType.FLEXIBLE);
    }

    private void setupUpdateDownloaded() {
        mFakeAppUpdateManager.setUpdateAvailable(999999, AppUpdateType.FLEXIBLE);
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();
        mFakeAppUpdateManager.startUpdateFlowForResult(
                info,
                mActivity,
                AppUpdateOptions.defaultOptions(AppUpdateType.FLEXIBLE),
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE);
        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();
        mFakeAppUpdateManager.downloadCompletes();
    }

    // =============================================================================================
    // Journey 1: Discovery Prompt & Acceptance
    // =============================================================================================

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_featureDisabled_doesNothing() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_availableShowsSnackbar() {
        setupUpdateAvailable();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

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
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_cannotShowSnackbar_doesNothing() {
        when(mSnackbarManager.canShowSnackbar()).thenReturn(false);
        setupUpdateAvailable();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .build();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_tapDiscoveryAction_startsFlexibleFlow() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                                InAppUpdateSnackbarEvent.ACTION_TAPPED)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.FLOW_STARTED)
                        .build();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertEquals(
                Integer.valueOf(AppUpdateType.FLEXIBLE),
                mFakeAppUpdateManager.getTypeForUpdateInProgress());
    }

    // =============================================================================================
    // Journey 2: Background Download Progress & Results
    // =============================================================================================

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInstallListener_downloadSucceeded() {
        setupUpdateAvailable();
        // Seed a restart backoff before starting to verify it gets cleared upon download
        // completion.
        InAppUpdatePolicy.recordRestartDeclined();
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(Snackbar.UMA_IN_APP_UPDATE_DISCOVERY, snackbar.getIdentifierForTesting());
        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.SUCCEEDED);

        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();
        mFakeAppUpdateManager.downloadCompletes();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());

        // Verify restart backoff is cleared and restart snackbar is displayed.
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
        ArgumentCaptor<Snackbar> allSnackbarsCaptor = ArgumentCaptor.forClass(Snackbar.class);
        verify(mSnackbarManager, times(2)).showSnackbar(allSnackbarsCaptor.capture());
        assertEquals(
                Snackbar.UMA_IN_APP_UPDATE_RESTART,
                allSnackbarsCaptor.getAllValues().get(1).getIdentifierForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInstallListener_downloadFails() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        mSnackbarCaptor.getValue().getController().onAction(null);
        ShadowLooper.idleMainLooper();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.DOWNLOAD_FAILED);

        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();
        mFakeAppUpdateManager.downloadFails();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testInstallListener_downloadCanceled() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        mSnackbarCaptor.getValue().getController().onAction(null);
        ShadowLooper.idleMainLooper();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.DOWNLOAD_CANCELED);

        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();
        mFakeAppUpdateManager.userCancelsDownload();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCancellationBackoff_suppressesDiscoveryPrompt() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        mSnackbarCaptor.getValue().getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getInstallListenerForTesting());
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
        assertTrue(InAppUpdatePolicy.isEligible(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.PROMPT_CANCELED);

        // Record a user cancellation result.
        controller.handleActivityResult(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_CANCELED);

        watcher.assertExpected();

        // Active listener is cleaned up on cancellation.
        assertNull(controller.getInstallListenerForTesting());
        // Automated discovery prompt is now throttled.
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
        // But general eligibility remains true (so manual menu clicks work).
        assertTrue(InAppUpdatePolicy.isEligible(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testHandleActivityResult_failureRecordsMetricAndBackoff() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        mSnackbarCaptor.getValue().getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getInstallListenerForTesting());
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.PLAY_CORE_FAILED);

        controller.handleActivityResult(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE,
                ActivityResult.RESULT_IN_APP_UPDATE_FAILED);

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testStartFlow_startReturnsFalse_recordsStartFailedAndBackoff() throws Exception {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockInfo.updateAvailability()).thenReturn(UpdateAvailability.UPDATE_AVAILABLE);
        when(mockInfo.isUpdateTypeAllowed(any(AppUpdateOptions.class))).thenReturn(true);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        when(mockManager.startUpdateFlowForResult(any(), any(Activity.class), any(), anyInt()))
                .thenReturn(false);
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.START_FAILED);

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testStartFlow_sendIntentException_recordsSendIntentFailedAndBackoff()
            throws Exception {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockInfo.updateAvailability()).thenReturn(UpdateAvailability.UPDATE_AVAILABLE);
        when(mockInfo.isUpdateTypeAllowed(any(AppUpdateOptions.class))).thenReturn(true);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        when(mockManager.startUpdateFlowForResult(any(), any(Activity.class), any(), anyInt()))
                .thenThrow(new IntentSender.SendIntentException("test exception"));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                        InAppUpdateController.DownloadResult.SEND_INTENT_FAILED);

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    // =============================================================================================
    // Journey 3: Restart Prompt & App Relaunch
    // =============================================================================================

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_featureDisabled_doesNothing() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_downloadedShowsRestartSnackbar() {
        setupUpdateDownloaded();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

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
        assertTrue(InAppUpdatePolicy.isEligible(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testCheckAndStartUpdateFlow_downloadedWithDiscoveryBackoffActive_showsRestartSnackbar() {
        setupUpdateDownloaded();

        // Record discovery prompt declined, activating 24h discovery backoff.
        InAppUpdatePolicy.recordUpdateDeclined();
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();

        // Even though discovery backoff is active, the restart snackbar MUST still be shown.
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        assertEquals(
                Snackbar.UMA_IN_APP_UPDATE_RESTART,
                mSnackbarCaptor.getValue().getIdentifierForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_downloadingRegistersListenerAndCompletes() {
        setupUpdateAvailable();
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();
        mFakeAppUpdateManager.startUpdateFlowForResult(
                info,
                mActivity,
                AppUpdateOptions.defaultOptions(AppUpdateType.FLEXIBLE),
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE);
        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();

        InAppUpdateController controller = createController();
        assertNull(controller.getInstallListenerForTesting());

        controller.onResume();
        ShadowLooper.idleMainLooper();

        // Listener is registered because update is DOWNLOADING.
        assertNotNull(controller.getInstallListenerForTesting());

        // When download completes, listener displays restart snackbar.
        mFakeAppUpdateManager.downloadCompletes();
        ShadowLooper.idleMainLooper();

        assertNull(controller.getInstallListenerForTesting());
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        assertEquals(
                Snackbar.UMA_IN_APP_UPDATE_RESTART,
                mSnackbarCaptor.getValue().getIdentifierForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_tapRestartAction_completesUpdate() {
        setupUpdateDownloaded();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.ACTION_TAPPED);

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(mFakeAppUpdateManager.isInstallSplashScreenVisible());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_downloaded_doesNotShowDuplicateSnackbar() {
        setupUpdateDownloaded();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        verify(mSnackbarManager, times(1)).showSnackbar(any());

        // Subsequent onResume call should not show another restart snackbar.
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, times(1)).showSnackbar(any());
    }

    // =============================================================================================
    // Journey 4: Dismissals & Backoff Policies
    // =============================================================================================

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testShowDiscoverySnackbar_dismissNoAction_recordsBackoff() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        // User allows snackbar to time out passively without action.
        snackbar.getController().onDismissNoAction(null);
        watcher.assertExpected();

        // Passive timeout records backoff in InAppUpdatePolicy.
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testShowRestartSnackbar_dismissNoAction_recordsBackoff() {
        setupUpdateDownloaded();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_NO_ACTION);

        // User allows restart snackbar to time out passively without action.
        snackbar.getController().onDismissNoAction(null);
        watcher.assertExpected();

        // Passive timeout records restart backoff in InAppUpdatePolicy.
        assertFalse(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testLifecycleInterruption_recordsLifecycleEventAndSkipsBackoff() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Activity transitions to STOPPED (e.g. app backgrounded or screen rotated).
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.STOPPED);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        snackbar.getController().onDismissNoAction(null);
        watcher.assertExpected();

        // Lifecycle dismissal must NOT record 24h backoff.
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testConfigurationChange_recordsLifecycleEventAndSkipsBackoff() {
        ConfigurationChangingActivity activity =
                Robolectric.buildActivity(ConfigurationChangingActivity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        setupUpdateAvailable();

        InAppUpdateController controller = createController(activity);
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Activity is undergoing a configuration change (e.g. screen rotation).
        activity.setIsChangingConfigurations(true);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.DISMISSED_BY_LIFECYCLE);

        snackbar.getController().onDismissNoAction(null);
        watcher.assertExpected();

        // Configuration change dismissal must NOT record 24h backoff.
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(activity));
    }

    // =============================================================================================
    // Journey 5: Activity Teardown & Lifecycle Guards
    // =============================================================================================

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDestroy_dismissesActiveSnackbars() {
        setupUpdateAvailable();

        // Teardown dismissal suppresses dismissal telemetry (recording only the initial
        // IMPRESSION).
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT,
                        InAppUpdateSnackbarEvent.IMPRESSION);

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        controller.destroy();
        verify(mSnackbarManager).dismissSnackbars(any());

        // Simulate SnackbarManager notifying controller of dismissal during teardown.
        snackbar.getController().onDismissNoAction(/* actionData= */ null);

        // Teardown dismissal suppresses decline backoff and telemetry.
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDestroy_dismissesRestartSnackbarAndResetsState() {
        setupUpdateDownloaded();

        // Teardown dismissal suppresses dismissal telemetry; only the two IMPRESSIONs are recorded.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_RESTART_SNACKBAR_EVENT,
                                InAppUpdateSnackbarEvent.IMPRESSION,
                                InAppUpdateSnackbarEvent.IMPRESSION)
                        .build();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        controller.destroy();
        verify(mSnackbarManager).dismissSnackbars(any());

        // Simulate SnackbarManager notifying controller of dismissal during teardown.
        snackbar.getController().onDismissNoAction(/* actionData= */ null);

        // Teardown dismissal suppresses restart decline backoff and telemetry.
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));

        // Verify that after destroy(), resuming another controller can show restart snackbar again.
        InAppUpdateController newController = createController();
        newController.onResume();
        ShadowLooper.idleMainLooper();

        // Second showSnackbar call succeeds because state was reset.
        verify(mSnackbarManager, times(2)).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDestroy_unregistersInstallListener() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        mSnackbarCaptor.getValue().getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getInstallListenerForTesting());

        controller.destroy();
        assertNull(controller.getInstallListenerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDestroy_suppressesLateActionTap() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                InAppUpdateSnackbarController.HISTOGRAM_DISCOVERY_SNACKBAR_EVENT)
                        .expectNoRecords(InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT)
                        .build();

        controller.destroy();
        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNull(controller.getInstallListenerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_activityFinishing_doesNothing() {
        mActivity.finish();
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_activityFinishing_doesNothing() {
        mActivity.finish();
        setupUpdateDownloaded();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_activityFinishesBeforeCallback_doesNotShowSnackbar() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        // Activity begins finishing before main looper idles the callback.
        mActivity.finish();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_destroyBeforeCallback_doesNotShowSnackbar() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        controller.destroy();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testHandleActivityResult_resultOk_registersInstallListener() {
        InAppUpdateController controller = createController();
        assertNull(controller.getInstallListenerForTesting());

        controller.handleActivityResult(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK);

        assertNotNull(controller.getInstallListenerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_completeUpdateFailure_recordsFailureBackoff() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.DOWNLOADED);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        when(mockManager.completeUpdate())
                .thenReturn(Tasks.forException(new RuntimeException("Disk full")));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
        assertTrue(InAppUpdatePolicy.isRestartPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_failure_doesNotDropExistingListener() {
        InAppUpdateController controller = createController();
        controller.handleActivityResult(
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE, Activity.RESULT_OK);
        assertNotNull(controller.getInstallListenerForTesting());

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo())
                .thenReturn(Tasks.forException(new RuntimeException("Play Store unavailable")));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getInstallListenerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_activityChangingConfigurations_doesNothing() {
        ConfigurationChangingActivity activity =
                Robolectric.buildActivity(ConfigurationChangingActivity.class).setup().get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        activity.setIsChangingConfigurations(true);
        setupUpdateAvailable();

        InAppUpdateController controller = createController(activity);
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testOnResume_tapRestartAction_activityDestroyedBeforeAction_doesNotCompleteUpdate() {
        setupUpdateDownloaded();

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();

        // Destroy controller before action is triggered.
        controller.destroy();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertFalse(mFakeAppUpdateManager.isInstallSplashScreenVisible());
    }

    private static class ConfigurationChangingActivity extends Activity {
        private boolean mIsChangingConfigurations;

        void setIsChangingConfigurations(boolean isChangingConfigurations) {
            mIsChangingConfigurations = isChangingConfigurations;
        }

        @Override
        public boolean isChangingConfigurations() {
            return mIsChangingConfigurations;
        }
    }
}
