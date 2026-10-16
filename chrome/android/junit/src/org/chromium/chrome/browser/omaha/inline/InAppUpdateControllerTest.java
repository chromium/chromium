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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.IntentSender;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.OnSuccessListener;
import com.google.android.gms.tasks.Task;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.gms.ChromiumPlayServicesAvailability;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Sociable module-level tests for the in-app update flow, testing {@link InAppUpdateController},
 * {@link InAppUpdateSnackbarController}, and {@link InAppUpdatePolicy} interactions.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class InAppUpdateControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SnackbarManager mSnackbarManager;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Captor private ArgumentCaptor<OnSuccessListener<AppUpdateInfo>> mUpdateInfoSuccessCaptor;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private FakeAppUpdateManager mFakeAppUpdateManager;

    @Before
    public void setUp() {
        InAppUpdateController.resetResumeCooldownForTesting();
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
        if (mActivityController != null && !mActivity.isDestroyed()) {
            mActivityController.destroy();
        }
        InAppUpdateController.setAppUpdateManagerForTesting(null);
        InAppUpdateController.resetResumeCooldownForTesting();
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
    public void testCheckAndStartUpdateFlow_nullSnackbarManager_doesNothing() {
        setupUpdateAvailable();

        InAppUpdateController controller = new InAppUpdateController(mActivity, () -> null);
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testCheckAndStartUpdateFlow_activityDestroyed_doesNothing() {
        setupUpdateAvailable();
        mActivityController.destroy();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, never()).showSnackbar(any());
        assertNull(mFakeAppUpdateManager.getTypeForUpdateInProgress());
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

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.FLOW_STARTED)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.SUCCEEDED)
                        .build();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

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
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.FLOW_STARTED)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.DOWNLOAD_FAILED)
                        .build();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

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
        Snackbar snackbar = mSnackbarCaptor.getValue();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.FLOW_STARTED)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.DOWNLOAD_CANCELED)
                        .build();

        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

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
    public void testInstallListener_activityFinishing_ignoresUpdate() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        Snackbar snackbar = mSnackbarCaptor.getValue();
        assertEquals(Snackbar.UMA_IN_APP_UPDATE_DISCOVERY, snackbar.getIdentifierForTesting());
        snackbar.getController().onAction(null);
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getInstallListenerForTesting());

        mActivity.finish();

        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();
        mFakeAppUpdateManager.downloadCompletes();
        ShadowLooper.idleMainLooper();

        verify(mSnackbarManager, times(1)).showSnackbar(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testHandleActivityResult_canceled() {
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

        // Calling handleActivityResult with an unrecognized request code is a safe no-op.
        controller.handleActivityResult(99999, Activity.RESULT_OK);
        assertNotNull(controller.getInstallListenerForTesting());

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
        ActivityController<ConfigurationChangingActivity> activityController =
                Robolectric.buildActivity(ConfigurationChangingActivity.class).setup();
        try {
            ConfigurationChangingActivity activity = activityController.get();
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
        } finally {
            activityController.destroy();
        }
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
        InAppUpdateController.resetResumeCooldownForTesting();
        InAppUpdateController newController = createController();
        newController.onResume();
        ShadowLooper.idleMainLooper();

        // Second showSnackbar call succeeds because state was reset.
        verify(mSnackbarManager, times(2)).showSnackbar(any());
        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testDestroy_unregistersListenerAndDismissesSnackbars() {
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

        // Repeated teardown is idempotent and safe.
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
    public void testOnMenuItemClicked_downloadAcceptedStartsFlexibleFlow() {
        setupUpdateAvailable();

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.TRIGGER_DOWNLOAD)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.FLOW_STARTED)
                        .build();

        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertEquals(
                Integer.valueOf(AppUpdateType.FLEXIBLE),
                mFakeAppUpdateManager.getTypeForUpdateInProgress());
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
    public void testOnMenuItemClicked_downloadedCompletesUpdate() {
        setupUpdateDownloaded();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                        InAppUpdateController.AppMenuAction.TRIGGER_RESTART);

        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(mFakeAppUpdateManager.isInstallSplashScreenVisible());
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
    public void testOnMenuItemClicked_downloading_registersListener() {
        setupUpdateAvailable();
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();
        mFakeAppUpdateManager.startUpdateFlowForResult(
                info,
                mActivity,
                AppUpdateOptions.defaultOptions(AppUpdateType.FLEXIBLE),
                InAppUpdateController.REQUEST_CODE_IN_APP_UPDATE);
        mFakeAppUpdateManager.userAcceptsUpdate();
        mFakeAppUpdateManager.downloadStarts();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                        InAppUpdateController.AppMenuAction.ALREADY_DOWNLOADING);

        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertNotNull(controller.getInstallListenerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_duplicateTapWhileFlowStarting_debounced() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        @SuppressWarnings("unchecked")
        Task<AppUpdateInfo> mockTask = mock(Task.class);
        @SuppressWarnings("unchecked")
        ArgumentCaptor<OnCompleteListener<AppUpdateInfo>> completeCaptor =
                ArgumentCaptor.forClass(OnCompleteListener.class);
        when(mockTask.addOnCompleteListener(completeCaptor.capture())).thenReturn(mockTask);
        when(mockTask.addOnSuccessListener(any(Activity.class), mUpdateInfoSuccessCaptor.capture()))
                .thenReturn(mockTask);
        when(mockTask.addOnFailureListener(any(Activity.class), any())).thenReturn(mockTask);
        when(mockManager.getAppUpdateInfo()).thenReturn(mockTask);
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();

        // First click begins flow and sets mIsFlowStarting = true.
        controller.onMenuItemClicked(null);

        // Second click while first is pending should be debounced without querying manager again.
        controller.onMenuItemClicked(null);

        verify(mockManager, times(1)).getAppUpdateInfo();

        // Resolve the pending task and verify mIsFlowStarting resets so subsequent clicks proceed.
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockInfo.updateAvailability()).thenReturn(UpdateAvailability.UPDATE_NOT_AVAILABLE);
        completeCaptor.getValue().onComplete(mockTask);
        mUpdateInfoSuccessCaptor.getValue().onSuccess(mockInfo);
        ShadowLooper.idleMainLooper();

        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(2)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_activityStoppedWhilePending_resetsFlowStartingOnCompletion() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        @SuppressWarnings("unchecked")
        Task<AppUpdateInfo> mockTask = mock(Task.class);
        @SuppressWarnings("unchecked")
        ArgumentCaptor<OnCompleteListener<AppUpdateInfo>> completeCaptor =
                ArgumentCaptor.forClass(OnCompleteListener.class);
        when(mockTask.addOnCompleteListener(completeCaptor.capture())).thenReturn(mockTask);
        when(mockTask.addOnSuccessListener(any(Activity.class), any())).thenReturn(mockTask);
        when(mockTask.addOnFailureListener(any(Activity.class), any())).thenReturn(mockTask);
        when(mockManager.getAppUpdateInfo()).thenReturn(mockTask);
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();

        // First click begins flow and sets mIsFlowStarting = true.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(1)).getAppUpdateInfo();

        // While task is pending, Activity is stopped (user backgrounds Chrome).
        mActivityController.pause().stop();

        // Second click while activity is stopped or pending is debounced.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(1)).getAppUpdateInfo();

        // Task completes in the background (invoking the unscoped OnCompleteListener).
        completeCaptor.getValue().onComplete(mockTask);
        ShadowLooper.idleMainLooper();

        // Activity is resumed.
        mActivityController.restart().start().resume();

        // Second click after task completion should now proceed rather than being permanently
        // debounced.
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(2)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_doesNotResetFlowStartingWhilePending() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        @SuppressWarnings("unchecked")
        Task<AppUpdateInfo> mockTask = mock(Task.class);
        @SuppressWarnings("unchecked")
        ArgumentCaptor<OnCompleteListener<AppUpdateInfo>> completeCaptor =
                ArgumentCaptor.forClass(OnCompleteListener.class);
        when(mockTask.addOnCompleteListener(completeCaptor.capture())).thenReturn(mockTask);
        when(mockTask.addOnSuccessListener(any(Activity.class), any())).thenReturn(mockTask);
        when(mockTask.addOnFailureListener(any(Activity.class), any())).thenReturn(mockTask);
        when(mockManager.getAppUpdateInfo()).thenReturn(mockTask);
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();

        // First click begins flow and sets mIsFlowStarting = true.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(1)).getAppUpdateInfo();

        // Second click is debounced.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(1)).getAppUpdateInfo();

        // Background and foreground Activity without the task completing.
        mActivityController.pause().stop();
        mActivityController.restart().start().resume();
        controller.onResume();
        // onResume invokes getAppUpdateInfo() for resume check, but does NOT reset mIsFlowStarting.
        verify(mockManager, times(2)).getAppUpdateInfo();

        // onResume does not reset mIsFlowStarting while task is pending; clicks remain debounced.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(2)).getAppUpdateInfo();

        // Complete the pending task to reset mIsFlowStarting to false.
        completeCaptor.getValue().onComplete(mockTask);
        ShadowLooper.idleMainLooper();

        // After completion, mIsFlowStarting is reset to false, allowing subsequent clicks.
        controller.onMenuItemClicked(null);
        verify(mockManager, times(3)).getAppUpdateInfo();
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
    public void testOnMenuItemClicked_unavailableTriggersFallback() {
        mFakeAppUpdateManager.setUpdateNotAvailable();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                        InAppUpdateController.AppMenuAction.INTENT_FALLBACK);

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_ineligibleTriggersFallbackWithoutLoggingHistogram() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(InAppUpdateController.HISTOGRAM_APP_MENU_ACTION)
                        .build();

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();
        assertTrue(fallbackCalled.get());

        // Passing null fallback must not throw NullPointerException.
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_unavailableWithNullFallback_doesNotLogFallbackHistogram() {
        mFakeAppUpdateManager.setUpdateNotAvailable();
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(InAppUpdateController.HISTOGRAM_APP_MENU_ACTION)
                        .build();

        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
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
        ActivityController<ConfigurationChangingActivity> activityController =
                Robolectric.buildActivity(ConfigurationChangingActivity.class).setup();
        try {
            ConfigurationChangingActivity activity = activityController.get();
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
            activity.setIsChangingConfigurations(true);
            setupUpdateAvailable();

            InAppUpdateController controller = createController(activity);
            controller.checkAndStartUpdateFlow();
            ShadowLooper.idleMainLooper();

            verify(mSnackbarManager, never()).showSnackbar(any());
        } finally {
            activityController.destroy();
        }
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

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testShowRestartSnackbar_dismissesActiveDiscoverySnackbar() {
        setupUpdateAvailable();
        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getDiscoverySnackbarControllerForTesting());
        assertNull(controller.getRestartSnackbarControllerForTesting());

        // When the update transitions to downloaded, onResume triggers the restart prompt.
        setupUpdateDownloaded();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        // The stale discovery snackbar should be dismissed on teardown (suppressing false decline)
        // and the restart prompt should now be active.
        assertNull(controller.getDiscoverySnackbarControllerForTesting());
        assertNotNull(controller.getRestartSnackbarControllerForTesting());
        assertTrue(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testShowDiscoverySnackbar_restartSnackbarActive_doesNotShowDiscovery() {
        setupUpdateDownloaded();
        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getRestartSnackbarControllerForTesting());

        // Calling checkAndStartUpdateFlow while restart snackbar is active must not show discovery.
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        assertNull(controller.getDiscoverySnackbarControllerForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_restartSnackbarActive_doesNotQueryAppUpdateInfo() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.DOWNLOADED);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        assertNotNull(controller.getRestartSnackbarControllerForTesting());
        verify(mockManager, times(1)).getAppUpdateInfo();

        // Second onResume while restart prompt is active should return early without IPC.
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(1)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_throttledByCooldown_doesNotQueryAppUpdateInfo() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(1)).getAppUpdateInfo();

        // Immediate second onResume call should be throttled by cooldown.
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(1)).getAppUpdateInfo();

        // Advance timestamp past cooldown (default 5 minutes).
        InAppUpdateController.setLastResumeCheckTimestampMsForTesting(
                TimeUtils.elapsedRealtimeMillis() - TimeUnit.MINUTES.toMillis(6));
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(2)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnResume_cooldownPersistsAcrossInstancesAndResets() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller1 = createController();
        controller1.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(1)).getAppUpdateInfo();

        // Simulate activity recreation (e.g. rotation): a new controller instance is created.
        InAppUpdateController controller2 = createController();
        controller2.onResume();
        ShadowLooper.idleMainLooper();

        // Second instance should be throttled by static resume cooldown.
        verify(mockManager, times(1)).getAppUpdateInfo();

        // Calling resetResumeCooldownForTesting() clears the cooldown, allowing onResume to
        // proceed.
        InAppUpdateController.resetResumeCooldownForTesting();
        controller2.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(2)).getAppUpdateInfo();

        // New instance after cooldown expires (e.g. advance 6 minutes) should also query Play Core.
        InAppUpdateController controller3 = createController();
        InAppUpdateController.setLastResumeCheckTimestampMsForTesting(
                TimeUtils.elapsedRealtimeMillis() - TimeUnit.MINUTES.toMillis(6));
        controller3.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(3)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    @CommandLineFlags.Add({InAppUpdatePolicy.FORCE_IN_APP_UPDATE})
    public void testOnResume_forceUpdate_bypassesCooldown() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(1)).getAppUpdateInfo();

        // Immediate second onResume call should NOT be throttled because force update is active.
        controller.onResume();
        ShadowLooper.idleMainLooper();

        verify(mockManager, times(2)).getAppUpdateInfo();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_activityChangingConfigurations_doesNothing() {
        ActivityController<ConfigurationChangingActivity> activityController =
                Robolectric.buildActivity(ConfigurationChangingActivity.class).setup();
        try {
            ConfigurationChangingActivity activity = activityController.get();
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
            activity.setIsChangingConfigurations(true);

            setupUpdateAvailable();

            AtomicBoolean fallbackCalled = new AtomicBoolean(false);
            InAppUpdateController controller = createController(activity);
            controller.onMenuItemClicked(() -> fallbackCalled.set(true));
            ShadowLooper.idleMainLooper();

            assertFalse(fallbackCalled.get());
            verify(mSnackbarManager, never()).showSnackbar(any());
        } finally {
            activityController.destroy();
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_typeNotAllowed_triggersFallback() {
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.UNKNOWN);
        when(mockInfo.updateAvailability()).thenReturn(UpdateAvailability.UPDATE_AVAILABLE);
        when(mockInfo.isUpdateTypeAllowed(any())).thenReturn(false);

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                        InAppUpdateController.AppMenuAction.INTENT_FALLBACK);

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_flowStartFails_triggersFallback() throws Exception {
        setupUpdateAvailable();
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(info));
        when(mockManager.startUpdateFlowForResult(any(), any(Activity.class), any(), anyInt()))
                .thenReturn(false);
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.INTENT_FALLBACK)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.START_FAILED)
                        .build();

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void
            testOnMenuItemClicked_sendIntentException_recordsSendIntentFailedAndTriggersFallback()
                    throws Exception {
        setupUpdateAvailable();
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(info));
        doThrow(new IntentSender.SendIntentException("Intent failed"))
                .when(mockManager)
                .startUpdateFlowForResult(any(), any(Activity.class), any(), anyInt());
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.INTENT_FALLBACK)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.SEND_INTENT_FAILED)
                        .build();

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_runtimeException_recordsStartExceptionAndTriggersFallback()
            throws Exception {
        setupUpdateAvailable();
        AppUpdateInfo info = mFakeAppUpdateManager.getAppUpdateInfo().getResult();

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(info));
        doThrow(new RuntimeException("Crash"))
                .when(mockManager)
                .startUpdateFlowForResult(any(), any(Activity.class), any(), anyInt());
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.INTENT_FALLBACK)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_DOWNLOAD_RESULT,
                                InAppUpdateController.DownloadResult.START_EXCEPTION)
                        .build();

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_completeUpdateFails_triggersFallback() {
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.DOWNLOADED);

        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        when(mockManager.completeUpdate())
                .thenReturn(Tasks.forException(new RuntimeException("Complete failed")));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.TRIGGER_RESTART)
                        .expectIntRecord(
                                InAppUpdateController.HISTOGRAM_APP_MENU_ACTION,
                                InAppUpdateController.AppMenuAction.RESTART_FAILED)
                        .build();

        AtomicBoolean fallbackCalled = new AtomicBoolean(false);
        InAppUpdateController controller = createController();
        controller.onMenuItemClicked(() -> fallbackCalled.set(true));
        ShadowLooper.idleMainLooper();

        watcher.assertExpected();
        assertTrue(fallbackCalled.get());
        assertFalse(InAppUpdatePolicy.isDiscoveryPromptAllowed(mActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_downloadedWithActiveRestartSnackbar_dismissesSnackbar() {
        AppUpdateManager mockManager = mock(AppUpdateManager.class);
        AppUpdateInfo mockInfo = mock(AppUpdateInfo.class);
        when(mockInfo.installStatus()).thenReturn(InstallStatus.DOWNLOADED);
        when(mockManager.getAppUpdateInfo()).thenReturn(Tasks.forResult(mockInfo));
        when(mockManager.completeUpdate()).thenReturn(Tasks.forResult(null));
        InAppUpdateController.setAppUpdateManagerForTesting(mockManager);

        InAppUpdateController controller = createController();
        controller.onResume();
        ShadowLooper.idleMainLooper();

        InAppUpdateSnackbarController restartController =
                controller.getRestartSnackbarControllerForTesting();
        assertNotNull(restartController);

        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        assertNull(controller.getRestartSnackbarControllerForTesting());
        verify(mSnackbarManager).dismissSnackbars(eq(restartController));
        verify(mockManager).completeUpdate();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.IN_APP_UPDATE_FLOW)
    public void testOnMenuItemClicked_activeDiscoverySnackbar_dismissesDiscoverySnackbar() {
        setupUpdateAvailable();

        InAppUpdateController controller = createController();
        controller.checkAndStartUpdateFlow();
        ShadowLooper.idleMainLooper();

        InAppUpdateSnackbarController discoveryController =
                controller.getDiscoverySnackbarControllerForTesting();
        assertNotNull(discoveryController);

        controller.onMenuItemClicked(null);
        ShadowLooper.idleMainLooper();

        assertNull(controller.getDiscoverySnackbarControllerForTesting());
        verify(mSnackbarManager).dismissSnackbars(eq(discoveryController));
    }
}
