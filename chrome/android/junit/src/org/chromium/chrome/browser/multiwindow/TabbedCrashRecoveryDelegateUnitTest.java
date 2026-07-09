// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabbedCrashRecoveryDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 30)
@EnableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
public class TabbedCrashRecoveryDelegateUnitTest {
    private static final int HOST_WINDOW_ID = 0;
    private static final int TEST_WINDOW_WIDTH = 800;
    private static final int TEST_WINDOW_HEIGHT = 600;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ChromeTabbedActivity mHostActivity;
    @Mock private Resources mResources;

    private TabbedCrashRecoveryDelegate mDelegate;
    private SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private List<CrashRecoveryWindowInfo> mCrashedWindows;
    private List<AppTask> mPreRecoveryAppTasks;

    @Before
    public void setUp() {
        TabbedCrashRecoveryDelegate.setInstanceForTesting(null);
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mDelegate = TabbedCrashRecoveryDelegate.getInstance();

        mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
        mModalDialogManagerSupplier.set(mModalDialogManager);
        when(mHostActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        when(mHostActivity.getResources()).thenReturn(mResources);
        ApplicationStatus.onStateChangeForTesting(mHostActivity, ActivityState.CREATED);
        when(mHostActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        when(mHostActivity.getWindowId()).thenReturn(HOST_WINDOW_ID);
        mCrashedWindows = new ArrayList<>();
        // Include the recovered host window in the list of crashed windows.
        mCrashedWindows.add(new CrashRecoveryWindowInfo(HOST_WINDOW_ID, /* isVisible= */ true));
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        ChromeMultiInstancePersistentStore.writeIsVisible(HOST_WINDOW_ID, true);
        ChromeMultiInstancePersistentStore.writeTabCount(HOST_WINDOW_ID, 1, 0);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(HOST_WINDOW_ID, true);
    }

    @After
    public void tearDown() {
        mDelegate.resetState();
        ChromeMultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_singleWindow_skipsRecoveryPrompt() {
        // Setup: Host window is the only window.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 0,
                /* numNonDefaultDisplayWindows= */ 0);
        writeCrashExitReasonToPrefs();

        var initWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.CrashRecoveryWindowCount", 1)
                        .build();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        initWatcher.assertExpected();
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_singleWindowNotHost_triggersDialog() {
        // Setup: Clear host window from crashed windows.
        mCrashedWindows.clear();
        ChromeMultiInstancePersistentStore.resetForTesting();
        ChromeMultiInstancePersistentStore.ensureInitialized();
        // Setup: Only 1 crashed window, and it is NOT the host window (say window 1).
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(1);
        ChromeMultiInstancePersistentStore.writeTabCount(1, 1, 0);
        ChromeMultiInstancePersistentStore.writeIsVisible(1, true);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(1, true);
        mCrashedWindows.add(new CrashRecoveryWindowInfo(1, /* isVisible= */ true));

        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertTrue(shown);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_singleWindowNotHostWithLiveTask_skipsRecoveryPrompt() {
        // Setup: Clear host window from crashed windows.
        mCrashedWindows.clear();
        ChromeMultiInstancePersistentStore.resetForTesting();
        ChromeMultiInstancePersistentStore.ensureInitialized();
        // Setup: Only 1 crashed window, and it is NOT the host window (say window 1).
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(1);
        ChromeMultiInstancePersistentStore.writeTabCount(1, 1, 0);
        ChromeMultiInstancePersistentStore.writeIsVisible(1, true);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(1, true);
        mCrashedWindows.add(new CrashRecoveryWindowInfo(1, /* isVisible= */ true));

        // Setup: Window 1 has a live task.
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);

        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_singleWindowHost_resetsState() {
        // Setup: Host window is the only window.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 0,
                /* numNonDefaultDisplayWindows= */ 0);
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();

        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertFalse(shown);

        // Now verify it's no longer eligible (resetState was called).
        // A second call should return false because mIsCrashRecoveryEligible was reset to false.
        assertFalse(
                "Delegate should no longer be eligible after state reset.",
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity));
    }

    @Test
    public void testDidLastSessionCrashWithRecoverableWindows_variousExitReasons() {
        // Setup: At least one crashed window exists on disk.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Crash reasons.
        int[] crashReasons = {
            ApplicationExitInfo.REASON_CRASH,
            ApplicationExitInfo.REASON_CRASH_NATIVE,
            ApplicationExitInfo.REASON_ANR
        };

        // Act & Verify.
        for (int reason : crashReasons) {
            mDelegate.resetState();
            writeExitReasonToPrefs(reason);
            assertTrue(
                    "Reason " + reason + " should need crash recovery.",
                    mDelegate.didLastSessionCrashWithRecoverableWindows());
        }

        // Setup: Non-crash reasons.
        int[] nonCrashReasons = {
            ApplicationExitInfo.REASON_USER_REQUESTED,
            ApplicationExitInfo.REASON_EXIT_SELF,
            ApplicationExitInfo.REASON_USER_STOPPED,
            -1 // API failure
        };

        // Act & Verify.
        for (int reason : nonCrashReasons) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(HOST_WINDOW_ID, true);
            ChromeMultiInstancePersistentStore.writeIsRecoverable(1, true);

            mDelegate.resetState();
            writeExitReasonToPrefs(reason);
            assertFalse(
                    "Reason " + reason + " should not need crash recovery.",
                    mDelegate.didLastSessionCrashWithRecoverableWindows());
            assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
            assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        }
    }

    @Test
    @Config(sdk = 29)
    public void testDidLastSessionCrashWithRecoverableWindows_preAndroidR_returnsFalse() {
        // Setup: At least one crashed window exists on disk.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        writeCrashExitReasonToPrefs();

        assertFalse(
                "Crash recovery should not be needed on SDK < 30.",
                mDelegate.didLastSessionCrashWithRecoverableWindows());
    }

    @Test
    public void testDidLastSessionCrashWithRecoverableWindows_noCrashedWindows_returnsFalse() {
        // Setup: No crashed windows on disk.
        mCrashedWindows.clear();
        ChromeMultiInstancePersistentStore.resetForTesting();
        writeCrashExitReasonToPrefs();

        // Act & Verify.
        assertFalse(
                "Crash recovery should not be needed when there are no crashed windows.",
                mDelegate.didLastSessionCrashWithRecoverableWindows());
    }

    @Test
    public void testMaybeDeferCrashRecovery_crashRecoveryNeeded_setsPending() {
        // Setup: At least one crashed window exists on disk, and crash exit reason in prefs.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.maybeDeferCrashRecovery();

        // Verify: Pending crash recovery is written.
        assertTrue(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    public void testMaybeDeferCrashRecovery_crashRecoveryNotNeeded_doesNotSetPending() {
        // Setup: No crashed windows, but crash exit reason in prefs.
        mCrashedWindows.clear();
        ChromeMultiInstancePersistentStore.resetForTesting();
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.maybeDeferCrashRecovery();

        // Verify: Pending crash recovery is not written.
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
    public void testInitializeCrashRecoveryMetadata_featureDisabled_noOp() {
        // Setup.
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();

        // Verify.
        assertFalse(
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity));
    }

    @Test
    public void testInitializeCrashRecoveryMetadata_nonCrashExitReason_noOp() {
        // Setup: At least one other crashed window exists on disk.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);

        // Setup: Write non-crash exit reason.
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeInt(
                ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON,
                ApplicationExitInfo.REASON_USER_REQUESTED);

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();

        // Verify.
        assertFalse(
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void testInitializeCrashRecoveryMetadata_pendingRecovery_triggersDialog() {
        // Setup: 1 host + 2 other windows = 3 total.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Simulate pending recovery.
        writeCrashExitReasonToPrefs();
        mDelegate.maybeDeferCrashRecovery();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();

        // Verify.
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);
        assertTrue(shown);
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    public void
            testInitializeCrashRecoveryMetadata_pendingRecoveryWithNonCrashExitReason_preservesMetadataAndTriggersDialog() {
        // Setup: 1 host + 2 other windows = 3 total.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);

        // Setup: Simulate pending recovery.
        writeCrashExitReasonToPrefs();
        mDelegate.maybeDeferCrashRecovery();

        // Setup: Write non-crash exit reason for the current session.
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeInt(
                ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON,
                ApplicationExitInfo.REASON_USER_REQUESTED);

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();

        // Verify: The recovery states of all windows are preserved (not cleared).
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        // Verify.
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);
        assertTrue(shown);
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_allOtherWindowsHaveLiveTasks_skipsRecoveryPrompt() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(0, 1, 2);
        writeCrashExitReasonToPrefs();

        var initWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.CrashRecoveryWindowCount", 3)
                        .build();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        initWatcher.assertExpected();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_allOtherWindowsHaveLiveTasksInMultiWindow_triggersDialog() {
        // Setup.
        when(mHostActivity.isInMultiWindowMode()).thenReturn(true);
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(0, 1, 2);
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify.
        assertTrue(shown);
        verify(mModalDialogManager).showDialog(any(), anyInt());
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void testRestoreWindows_restoresWindowsInNewTasks() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 1);
        setupPreRecoveryAppTasks(0);
        setupAndShowCrashRecoveryDialog();

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify.
        ArgumentCaptor<Intent> intentCaptor1 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor2 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor3 = ArgumentCaptor.forClass(Intent.class);

        InOrder inOrderVerifier = inOrder(mHostActivity);

        // Verify: Non-visible window is restored first.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor1.capture());
        Intent intent1 = intentCaptor1.getValue();
        assertEquals(1, intent1.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent1.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));

        // Verify: Window from default display is restored.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor2.capture());
        Intent intent2 = intentCaptor2.getValue();
        assertEquals(2, intent2.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent2.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        // Verify: Window from non-default display is restored.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor3.capture());
        Intent intent3 = intentCaptor3.getValue();
        assertEquals(3, intent3.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent3.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));
    }

    @Test
    public void testRestoreWindows_finishesOrphanedTask_hostWindowInMultiWindowMode() {
        // Setup.
        when(mHostActivity.isInMultiWindowMode()).thenReturn(true);
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Non-visible window with windowId=1 and visible window with windowId=3 have tasks
        // that sustained a crash.
        setupPreRecoveryAppTasks(0, 1, 3);
        setupAndShowCrashRecoveryDialog();

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify.
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        AppTask liveTask3 = mPreRecoveryAppTasks.get(2);

        ArgumentCaptor<Intent> intentCaptor1 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor2 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor3 = ArgumentCaptor.forClass(Intent.class);

        InOrder inOrderVerifier = inOrder(mHostActivity, liveTask1, liveTask3);

        // Verify: Non-visible window (windowId=1) task is finished and then restored.
        inOrderVerifier.verify(liveTask1).finishAndRemoveTask();
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor1.capture());
        assertEquals(1, intentCaptor1.getValue().getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intentCaptor1
                        .getValue()
                        .getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));

        // Verify: Visible window (windowId=2) is restored (no task to finish).
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor2.capture());
        assertEquals(2, intentCaptor2.getValue().getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intentCaptor2
                        .getValue()
                        .getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));

        // Verify: Visible window (windowId=3) task is finished and then restored.
        inOrderVerifier.verify(liveTask3).finishAndRemoveTask();
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor3.capture());
        assertEquals(3, intentCaptor3.getValue().getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intentCaptor3
                        .getValue()
                        .getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));

        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));
    }

    @Test
    public void testRestoreWindows_skipsWindowWithLiveTask_hostWindowNotInMultiWindowMode() {
        // Setup.
        when(mHostActivity.isInMultiWindowMode()).thenReturn(false);
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Non-visible window (windowId=1) has a live task.
        setupPreRecoveryAppTasks(0, 1);
        setupAndShowCrashRecoveryDialog();

        var expectedWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.MultiWindow.CrashRecoveryDuration")
                        .build();
        var userActionTester = new UserActionTester();

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify: The live task for the non-visible window should not be finished when host window
        // is launched in non-multi window mode.
        AppTask liveTask = mPreRecoveryAppTasks.get(1);
        verify(liveTask, never()).finishAndRemoveTask();

        // Verify: Only the visible window (windowId=2) should be started.
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mHostActivity).startActivity(intentCaptor.capture());
        mDelegate.registerRecovery(2);

        Intent intent = intentCaptor.getValue();
        assertNotNull(intent);
        assertEquals(2, intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        // Verify: Success metrics should be recorded.
        expectedWatcher.assertExpected();
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryCompleted"));
        userActionTester.tearDown();
    }

    @Test
    public void testInitiateAndRegisterRecovery_recordsMetrics() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 1);
        setupPreRecoveryAppTasks(0);

        var initWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.CrashRecoveryWindowCount", 3)
                        .build();
        setupAndShowCrashRecoveryDialog();
        initWatcher.assertExpected();

        var userActionTester = new UserActionTester();
        mDelegate.restoreWindows(mHostActivity);
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryInitiated"));
        var noRecordsWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.MultiWindow.CrashRecoveryDuration")
                        .build();

        // Act: Recover first window (windowId=1).
        mDelegate.registerRecovery(1);

        // Verify: Metrics should not be recorded yet because window 2 is still pending.
        noRecordsWatcher.assertExpected();
        assertFalse(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryCompleted"));
        var expectedWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.MultiWindow.CrashRecoveryDuration")
                        .build();

        // Act: Recover second window (windowId=2).
        mDelegate.registerRecovery(2);

        // Verify: All windows recovered, success metrics should be recorded.
        expectedWatcher.assertExpected();
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryCompleted"));

        userActionTester.tearDown();
    }

    @Test
    public void testRecoveryDialogProperties() {
        // Setup: 1 host + 2 other windows = 3 total crashed windows.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        var userActionTester = new UserActionTester();
        setupAndShowCrashRecoveryDialog();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryDialogShown"));

        assertEquals(
                mHostActivity.getString(R.string.crash_recovery_dialog_title),
                model.get(ModalDialogProperties.TITLE));
        assertEquals(
                mHostActivity.getString(R.string.crash_recovery_dialog_message),
                model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        assertEquals(
                mHostActivity.getString(R.string.crash_recovery_dialog_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_userOptIn() {
        // Setup: 1 host + 1 other window = 2 total crashed windows.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        // windowId=1 does NOT have a task, so dialog will be shown.
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        var userActionTester = new UserActionTester();

        setupAndShowCrashRecoveryDialog();

        // Capture the dialog model and controller.
        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Verify: Positive button text is correctly set.
        assertEquals(
                mHostActivity.getString(R.string.crash_recovery_dialog_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryDialogShown"));

        // Act: Simulate positive button click.
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        // Verify: Windows are restored.
        verify(mHostActivity).startActivity(any(Intent.class));
        assertTrue(
                userActionTester.getActions().contains("Android.MultiWindow.CrashRecoveryOptIn"));
        verify(mModalDialogManager)
                .dismissDialog(
                        any(PropertyModel.class), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));

        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_userOptOut() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        var userActionTester = new UserActionTester();

        setupAndShowCrashRecoveryDialog();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryDialogShown"));

        // Act: Simulate negative button click.
        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);

        // Verify: Dialog dismissed without restoration.
        verify(mModalDialogManager)
                .dismissDialog(
                        any(PropertyModel.class), eq(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED));
        verify(mHostActivity, never()).startActivity(any(Intent.class), any(Bundle.class));

        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_dismissalCleanup() {
        // Setup: windowId=1 has NO task initially, so dialog is shown.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0); // windows 1, 2.
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 2); // window 2 has a task, window 1 doesn't.
        var userActionTester = new UserActionTester();

        setupAndShowCrashRecoveryDialog();

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryDialogShown"));

        // Act: Simulate dismissal (e.g., via back button).
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        // Verify: State is cleaned up for window 1 and 2.
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        // Window 2 had a live task, verify it was finished.
        AppTask liveTask2 = mPreRecoveryAppTasks.get(1);
        verify(liveTask2).finishAndRemoveTask();

        userActionTester.tearDown();
    }

    private void setupOtherCrashedWindows(
            int numNonVisibleWindows,
            int numDefaultDisplayWindows,
            int numNonDefaultDisplayWindows) {
        int start = 1;
        int end = numNonVisibleWindows + 1;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(i);
            ChromeMultiInstancePersistentStore.writeTabCount(i, 1, 0);
            ChromeMultiInstancePersistentStore.writeIsVisible(i, false);
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            mCrashedWindows.add(new CrashRecoveryWindowInfo(i, /* isVisible= */ false));
        }
        start = end;
        end = start + numDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(i);
            ChromeMultiInstancePersistentStore.writeIsVisible(i, true);
            ChromeMultiInstancePersistentStore.writeTabCount(i, 1, 0);
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            mCrashedWindows.add(new CrashRecoveryWindowInfo(i, /* isVisible= */ true));
        }
        start = end;
        end = end + numNonDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(i);
            ChromeMultiInstancePersistentStore.writeIsVisible(i, true);
            ChromeMultiInstancePersistentStore.writeTabCount(i, 1, 0);
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            // Non-default display windows are visible pre-crash.
            mCrashedWindows.add(new CrashRecoveryWindowInfo(i, /* isVisible= */ true));
        }
    }

    private void setupPreRecoveryAppTasks(Integer... windowIds) {
        mPreRecoveryAppTasks = new ArrayList<>();
        for (int windowId : windowIds) {
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = windowId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            mPreRecoveryAppTasks.add(appTask);
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
            ChromeMultiInstancePersistentStore.writeTaskId(windowId, windowId);
        }
        when(mActivityManager.getAppTasks()).thenReturn(mPreRecoveryAppTasks);
    }

    private void writeCrashExitReasonToPrefs() {
        writeExitReasonToPrefs(ApplicationExitInfo.REASON_CRASH);
    }

    private void writeExitReasonToPrefs(int exitReason) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeInt(ChromePreferenceKeys.LAST_SESSION_BROWSER_EXIT_REASON, exitReason);
    }

    private void setupAndShowCrashRecoveryDialog() {
        writeCrashExitReasonToPrefs();
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);
        assertTrue(shown);
    }
}
