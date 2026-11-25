// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.FakeTimeTestRule;
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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.recent_tabs.RecentlyClosedEntriesManagerTracker;
import org.chromium.chrome.browser.recent_tabs.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabbedCrashRecoveryDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
@EnableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
public class TabbedCrashRecoveryDelegateUnitTest {
    /** Minimal subclass to control methods that require native/window initialization. */
    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        boolean mIsIncognitoWindow;

        @Override
        public int getWindowId() {
            return HOST_WINDOW_ID;
        }

        @Override
        public boolean isIncognitoWindow() {
            return mIsIncognitoWindow;
        }
    }

    private static final int HOST_WINDOW_ID = 0;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private RecentlyClosedEntriesManagerTracker mRecentlyClosedTracker;

    @Captor private ArgumentCaptor<List<InstanceInfo>> mClosedWindowsCaptor;

    private TestChromeTabbedActivity mHostActivity;
    private TabbedCrashRecoveryDelegate mDelegate;
    private SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private List<CrashRecoveryWindowInfo> mCrashedWindows;
    private List<AppTask> mPreRecoveryAppTasks;

    @Before
    public void setUp() {
        TabbedCrashRecoveryDelegate.setInstanceForTesting(null);
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        RecentlyClosedEntriesManagerTrackerFactory.setInstanceForTesting(mRecentlyClosedTracker);
        mDelegate = TabbedCrashRecoveryDelegate.getInstance();

        mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
        mModalDialogManagerSupplier.set(mModalDialogManager);
        mHostActivity = Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        ApplicationStatus.onStateChangeForTesting(mHostActivity, ActivityState.CREATED);
        mCrashedWindows = new ArrayList<>();
        setupCrashedWindow(
                HOST_WINDOW_ID, 1, 0, /* isVisible= */ true, SupportedProfileType.REGULAR);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
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
        setupCrashedWindow(1, /* isVisible= */ true);

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
        // Simulate a single non-host crashed window that already has a live OS task.
        mCrashedWindows.clear();
        ChromeMultiInstancePersistentStore.resetForTesting();
        ChromeMultiInstancePersistentStore.ensureInitialized();
        setupCrashedWindow(/* windowId= */ 1, /* isVisible= */ true);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Attempt to initialize metadata and show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped and window 1 stays active without being soft-closed.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        verify(mPreRecoveryAppTasks.get(1), never()).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(1));
        assertEquals(1, ChromeMultiInstancePersistentStore.readTaskId(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
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
            assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
            assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
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
        ChromeMultiInstancePersistentStore.ensureInitialized();
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
        ChromeMultiInstancePersistentStore.ensureInitialized();
        writeCrashExitReasonToPrefs();

        // Act.
        mDelegate.maybeDeferCrashRecovery();

        // Verify: Pending crash recovery is not written.
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
    }

    @Test
    public void testMaybeDeferCrashRecovery_nonCrashExitReason_doesNotClearRecoverableState() {
        // Setup: Recoverable windows exist, but exit reason in prefs is non-crash.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        writeExitReasonToPrefs(ApplicationExitInfo.REASON_USER_REQUESTED);

        // Act.
        mDelegate.maybeDeferCrashRecovery();

        // Verify: Pending crash recovery is not written and recoverable state is preserved.
        assertFalse(ChromeMultiInstancePersistentStore.readIsCrashRecoveryPending());
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
    public void testInitializeCrashRecoveryMetadata_featureDisabled_noOp() {
        // Setup.
        writeCrashExitReasonToPrefs();

        // Act & Verify.
        assertFalse(mDelegate.initializeCrashRecoveryMetadata());
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

        // Act & Verify.
        assertFalse(mDelegate.initializeCrashRecoveryMetadata());
        assertFalse(
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity));
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
        // Simulate non-multi-window mode where all non-host crashed windows already have live
        // tasks.
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

        // Initialize crash recovery metadata and attempt to show the prompt.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the dialog is skipped and live tasks remain active without soft-closure.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        initWatcher.assertExpected();
        verify(mPreRecoveryAppTasks.get(1), never()).finishAndRemoveTask();
        verify(mPreRecoveryAppTasks.get(2), never()).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(2));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(1));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(2));
        assertEquals(1, ChromeMultiInstancePersistentStore.readTaskId(1));
        assertEquals(2, ChromeMultiInstancePersistentStore.readTaskId(2));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_allOtherWindowsHaveLiveTasksInMultiWindow_triggersDialog() {
        // Setup.
        Shadows.shadowOf(mHostActivity).setInMultiWindowMode(true);
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
        mDelegate.restoreWindows(mHostActivity, MultiWindowUtils.getAppTasksById(mHostActivity));

        // Verify: Non-visible window (1), default display window (2), and non-default display
        // window (3) are restored in order.
        assertRestoredWindowIntents(1, 2, 3);
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));
    }

    @Test
    public void testRestoreWindows_finishesOrphanedTask_hostWindowInMultiWindowMode() {
        // Setup.
        Shadows.shadowOf(mHostActivity).setInMultiWindowMode(true);
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Non-visible window with windowId=1 and visible window with windowId=3 have tasks
        // that sustained a crash.
        setupPreRecoveryAppTasks(0, 1, 3);
        setupAndShowCrashRecoveryDialog();

        // Act.
        mDelegate.restoreWindows(mHostActivity, MultiWindowUtils.getAppTasksById(mHostActivity));

        // Verify: Tasks 1 and 3 are finished in order, and windows 1, 2, 3 are restored in order.
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        AppTask liveTask3 = mPreRecoveryAppTasks.get(2);
        InOrder inOrderVerifier = inOrder(liveTask1, liveTask3);
        inOrderVerifier.verify(liveTask1).finishAndRemoveTask();
        inOrderVerifier.verify(liveTask3).finishAndRemoveTask();
        assertRestoredWindowIntents(1, 2, 3);

        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));
    }

    @Test
    public void testRestoreWindows_skipsWindowWithLiveTask_hostWindowNotInMultiWindowMode() {
        // Setup.
        Shadows.shadowOf(mHostActivity).setInMultiWindowMode(false);
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
        mDelegate.restoreWindows(mHostActivity, MultiWindowUtils.getAppTasksById(mHostActivity));

        // Verify: The live task for the non-visible window should not be finished when host window
        // is launched in non-multi window mode.
        AppTask liveTask = mPreRecoveryAppTasks.get(1);
        verify(liveTask, never()).finishAndRemoveTask();

        // Verify: Only the visible window (windowId=2) should be started.
        assertRestoredWindowIntents(2);
        mDelegate.registerRestoration(2);

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
    public void testRestoreWindows_clearsStrongReferencesImmediately() {
        // Setup: 1 host + 1 visible window (Id 1).
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0); // windowId=1
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        setupAndShowCrashRecoveryDialog();

        // Act: Click positive (restore).
        mDelegate.restoreWindows(mHostActivity, MultiWindowUtils.getAppTasksById(mHostActivity));

        // Verify: Strong references are cleared immediately after restoreWindows returns.
        assertTrue(mDelegate.getNonVisibleWindowsForTesting().isEmpty());
        assertTrue(mDelegate.getVisibleWindowsForTesting().isEmpty());
        assertNull(mDelegate.getCrashedWindowsForTesting());
        assertFalse(mDelegate.isCrashRecoveryEligibleForTesting());

        // Verify: Pending restoration window set is NOT cleared yet because window 1 hasn't
        // registered restoration.
        assertEquals(1, mDelegate.getWindowIdsPendingRestorationForTesting().size());
        assertTrue(mDelegate.getWindowIdsPendingRestorationForTesting().contains(1));

        // Act: Register restoration for window 1 via TabbedStartupCoordinator.
        TabbedStartupCoordinator.onWindowCreated(
                /* windowId= */ 1, NewWindowAppSource.CRASH_RECOVERY);

        // Verify: Pending restoration window set is now empty (fully reset).
        assertTrue(mDelegate.getWindowIdsPendingRestorationForTesting().isEmpty());
    }

    @Test
    public void testInitiateAndRegisterRestoration_recordsMetrics() {
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
        mDelegate.restoreWindows(mHostActivity, MultiWindowUtils.getAppTasksById(mHostActivity));
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryInitiated"));
        var noRecordsWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.MultiWindow.CrashRecoveryDuration")
                        .build();

        // Act: Recover first window (windowId=1).
        mDelegate.registerRestoration(1);

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
        mDelegate.registerRestoration(2);

        // Verify: All windows recovered, success metrics should be recorded.
        expectedWatcher.assertExpected();
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryCompleted"));
        assertStateReset();
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
        // Simulate 1 host window and 1 recoverable window without a live task so the dialog shows.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        var userActionTester = new UserActionTester();

        setupAndShowCrashRecoveryDialog();

        // Capture the dialog model and controller.
        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(modelCaptor.capture(), eq(ModalDialogManager.ModalDialogType.APP));
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Verify the positive button text and dialog-shown action are recorded.
        assertEquals(
                mHostActivity.getString(R.string.crash_recovery_dialog_positive_button_text),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.MultiWindow.CrashRecoveryDialogShown"));

        // Click the positive button to opt into restoring crashed windows.
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        controller.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        // Verify windows are restored without soft-closing them into Recent Tabs.
        assertNotNull(Shadows.shadowOf(mHostActivity).getNextStartedActivity());
        assertTrue(
                userActionTester.getActions().contains("Android.MultiWindow.CrashRecoveryOptIn"));
        verify(mModalDialogManager)
                .dismissDialog(
                        any(PropertyModel.class), eq(DialogDismissalCause.POSITIVE_BUTTON_CLICKED));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());

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
        assertNull(Shadows.shadowOf(mHostActivity).getNextStartedActivity());

        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_dismissalCleanup() {
        // Simulate 1 older non-visible crashed window without a live task (populated metadata) and
        // 1 more recently accessed visible crashed window with a live task (unset active tab
        // URL/title to verify empty string coalescing).
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 1, "https://one.example.com");
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(/* instanceId= */ 1, "One");
        ChromeMultiInstancePersistentStore.writeCustomTitle(/* instanceId= */ 1, "Custom One");
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 2);
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

        // Dismiss the recovery dialog without opting in (e.g. via back button).
        mFakeTimeTestRule.advanceMillis(100);
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        // Verify both non-visible and visible windows are batch soft-closed with one shared
        // closureTime, without marking for deletion, and synced to Recent Tabs sorted by
        // lastAccessedTime descending.
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(2));
        long closureTime = ChromeMultiInstancePersistentStore.readClosureTime(1);
        assertTrue(closureTime > 0);
        assertEquals(closureTime, ChromeMultiInstancePersistentStore.readClosureTime(2));

        // Verify window 2's surviving OS task is finished and removed.
        AppTask liveTask2 = mPreRecoveryAppTasks.get(1);
        verify(liveTask2).finishAndRemoveTask();

        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(2, closedWindows.size());

        InstanceInfo info2 = closedWindows.get(0);
        assertEquals(2, info2.instanceId);
        assertEquals(MultiInstanceManager.INVALID_TASK_ID, info2.taskId);
        assertEquals(InstanceInfo.Type.OTHER, info2.type);
        assertEquals("", info2.url);
        assertEquals("", info2.title);
        assertNull(info2.customTitle);
        assertEquals(1, info2.tabCount);
        assertEquals(closureTime, info2.closureTime);

        InstanceInfo info1 = closedWindows.get(1);
        assertEquals(1, info1.instanceId);
        assertEquals(MultiInstanceManager.INVALID_TASK_ID, info1.taskId);
        assertEquals(InstanceInfo.Type.OTHER, info1.type);
        assertEquals("https://one.example.com", info1.url);
        assertEquals("One", info1.title);
        assertEquals("Custom One", info1.customTitle);
        assertEquals(1, info1.tabCount);
        assertEquals(closureTime, info1.closureTime);
        assertTrue(info2.lastAccessedTime > info1.lastAccessedTime);

        assertStateReset();
        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_dismissalCleanup_withEmptyCrashedWindow() {
        // Simulate recoverable window 1 without a task and empty window 2 with a live task.
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupCrashedWindow(
                /* windowId= */ 2,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 2);
        var userActionTester = new UserActionTester();

        // Show the crash recovery dialog, which immediately cleans up empty window 2.
        setupAndShowCrashRecoveryDialog();

        // Verify empty window 2 had its task finished and recoverability cleared.
        AppTask liveTask2 = mPreRecoveryAppTasks.get(1);
        verify(liveTask2).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Dismiss the dialog without opting in.
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);

        // Verify only regular window 1 is soft-closed and reported to Recent Tabs, while window 2's
        // task is not finished a second time.
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertTrue(ChromeMultiInstancePersistentStore.readClosureTime(1) > 0);
        verify(liveTask2, times(1)).finishAndRemoveTask();

        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(1, closedWindows.size());
        assertEquals(1, closedWindows.get(0).instanceId);

        assertStateReset();
        userActionTester.tearDown();
    }

    @Test
    public void testShowRecoveryDialog_dismissalCleanup_withSingleNtpCrashedWindow() {
        // Simulate an older regular crashed window (1) and a more recently accessed single-NTP
        // crashed window (2) with a live task.
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 1, "https://one.example.com");
        setupCrashedWindow(
                /* windowId= */ 2,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 2, "chrome-native://newtab/");
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 2);

        // Show the crash recovery dialog and verify single-NTP window 2 remains eligible for
        // recovery until the user chooses whether to restore or dismiss.
        setupAndShowCrashRecoveryDialog();
        AppTask liveTask2 = mPreRecoveryAppTasks.get(1);
        verify(liveTask2, never()).finishAndRemoveTask();
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Dismiss the dialog without opting in and verify single-NTP window 2 is cleaned up
        // without soft-closing while regular window 1 is soft-closed and reported to Recent Tabs.
        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertTrue(ChromeMultiInstancePersistentStore.readClosureTime(1) > 0);
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(2));
        verify(liveTask2).finishAndRemoveTask();

        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(1, closedWindows.size());
        assertEquals(1, closedWindows.get(0).instanceId);

        assertStateReset();
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_incognitoHost_cancelsRecovery() {
        // Simulate an incognito host activity when two dead-task regular crashed windows exist.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 2,
                /* numDefaultDisplayWindows= */ 0,
                /* numNonDefaultDisplayWindows= */ 0);
        writeCrashExitReasonToPrefs();
        mHostActivity.mIsIncognitoWindow = true;

        // Initialize crash recovery metadata and verify initial recoverability state.
        mDelegate.initializeCrashRecoveryMetadata();
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(2));

        mFakeTimeTestRule.advanceMillis(100);
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped and both dead-task regular windows are batch soft-closed
        // and reported to Recent Tabs ordered by lastAccessedTime descending.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(HOST_WINDOW_ID));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(HOST_WINDOW_ID));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(2));
        long closureTime = ChromeMultiInstancePersistentStore.readClosureTime(1);
        assertTrue(closureTime > 0);
        assertEquals(closureTime, ChromeMultiInstancePersistentStore.readClosureTime(2));

        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(2, closedWindows.size());
        assertEquals(2, closedWindows.get(0).instanceId);
        assertEquals(1, closedWindows.get(1).instanceId);
        assertTrue(closedWindows.get(0).lastAccessedTime > closedWindows.get(1).lastAccessedTime);
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_incognitoHost_preservesLiveTaskAndSoftClosesDeadTask() {
        // Simulate an incognito host alongside a live-task regular window (1), a dead-task regular
        // window with stale task mapping (2), and a live-task empty window (3).
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupCrashedWindow(
                /* windowId= */ 2,
                /* normalTabCount= */ 2,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ false,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeTaskId(/* instanceId= */ 2, /* taskId= */ 22);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 2, "https://two.example.com");
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(/* instanceId= */ 2, "Two");
        setupCrashedWindow(
                /* windowId= */ 3,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1, 3);
        writeCrashExitReasonToPrefs();
        mHostActivity.mIsIncognitoWindow = true;

        // Attempt to show the crash recovery dialog on the incognito host window.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify live-task regular window 1 remains active without finishing its OS task.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1, never()).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(1));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(1));
        assertEquals(1, ChromeMultiInstancePersistentStore.readTaskId(1));

        // Verify dead-task regular window 2 is soft-closed with a valid closure timestamp.
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(2));
        long closureTime = ChromeMultiInstancePersistentStore.readClosureTime(2);
        assertTrue(closureTime > 0);

        // Verify empty window 3 has its live task finished and recoverability cleared.
        AppTask liveTask3 = mPreRecoveryAppTasks.get(2);
        verify(liveTask3).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));

        // Verify Recent Tabs is notified exclusively for dead-task regular window 2.
        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(1, closedWindows.size());
        InstanceInfo closedInfo = closedWindows.get(0);
        assertEquals(2, closedInfo.instanceId);
        assertEquals(MultiInstanceManager.INVALID_TASK_ID, closedInfo.taskId);
        assertEquals(InstanceInfo.Type.OTHER, closedInfo.type);
        assertEquals("https://two.example.com", closedInfo.url);
        assertEquals("Two", closedInfo.title);
        assertEquals(2, closedInfo.tabCount);
        assertEquals(closureTime, closedInfo.closureTime);
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_incognitoHost_skipsSoftClosingSingleNtpWindows() {
        // Simulate an incognito host alongside an older dead-task regular window (1), a newer
        // dead-task single-NTP window (2), and a live-task single-NTP window (3).
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ false,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 1, "https://one.example.com");
        setupCrashedWindow(
                /* windowId= */ 2,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 2, "chrome-native://newtab/");
        setupCrashedWindow(
                /* windowId= */ 3,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 3, "chrome-native://newtab/");
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 3);
        writeCrashExitReasonToPrefs();
        mHostActivity.mIsIncognitoWindow = true;

        // Attempt to show the crash recovery dialog on the incognito host window.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify live-task single-NTP window 3 keeps its OS task, neither single-NTP window
        // records closureTime, and only dead-task regular window 1 is soft-closed into Recent Tabs.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask3 = mPreRecoveryAppTasks.get(1);
        verify(liveTask3, never()).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(3));
        assertTrue(ChromeMultiInstancePersistentStore.readClosureTime(1) > 0);
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(2));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(3));

        verify(mRecentlyClosedTracker).onInstancesClosed(mClosedWindowsCaptor.capture(), eq(false));
        List<InstanceInfo> closedWindows = mClosedWindowsCaptor.getValue();
        assertEquals(1, closedWindows.size());
        assertEquals(1, closedWindows.get(0).instanceId);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMaybeShowCrashRecoveryDialog_incognitoCrashedWindow_cleanedUp() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        // Simulate a regular host window and an incognito-only crashed window with a live task.
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 1,
                /* isVisible= */ true,
                SupportedProfileType.OFF_THE_RECORD);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and attempt to show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped, the live task is finished, and recoverability is cleared.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
        assertStateReset();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void
            testMaybeShowCrashRecoveryDialog_incognitoHostAndIncognitoCrashedWindow_cleanedUp() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        // Simulate an incognito host window and another incognito-only crashed window with a live
        // task.
        mHostActivity.mIsIncognitoWindow = true;
        ChromeMultiInstancePersistentStore.writeProfileType(
                HOST_WINDOW_ID, SupportedProfileType.OFF_THE_RECORD);
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 1,
                /* isVisible= */ true,
                SupportedProfileType.OFF_THE_RECORD);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and attempt to show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped, the live task is finished, and recoverability is cleared.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
        assertStateReset();
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_emptyCrashedWindow_cleanedUp() {
        // Simulate a regular host window and an empty crashed window with a live task.
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and attempt to show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped, the live task is finished, and recoverability is cleared.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
        assertStateReset();
    }

    @Test
    public void
            testMaybeShowCrashRecoveryDialog_singleNtpCrashedWindow_triggersDialogAndRestores() {
        // Simulate a regular host window and a dead-task single-NTP crashed window (e.g. a tab
        // that may carry Back-stack navigation history).
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 1,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                /* instanceId= */ 1, "chrome-native://newtab/");
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and verify the crash recovery dialog is shown for the single-NTP
        // window.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);
        assertTrue(shown);
        assertTrue(ChromeMultiInstancePersistentStore.readIsRecoverable(1));

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Opt in to restore and verify the single-NTP window is restored without soft-closing into
        // Recent Tabs.
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        controller.onDismiss(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        assertRestoredWindowIntents(1);
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertEquals(0, ChromeMultiInstancePersistentStore.readClosureTime(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_mixedWindowWithIncognitoOnlyTabs_cleanedUp() {
        // Simulate a MIXED-profile crashed window containing only incognito tabs and a live task.
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 2,
                /* isVisible= */ true,
                SupportedProfileType.MIXED);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and attempt to show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped, the live task is finished, and recoverability is cleared.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
    }

    @Test
    public void testMaybeShowCrashRecoveryDialog_emptyCrashedWindowInMultiWindowMode_cleanedUp() {
        // Simulate multi-window mode with an empty crashed window holding a live task.
        Shadows.shadowOf(mHostActivity).setInMultiWindowMode(true);
        setupCrashedWindow(
                /* windowId= */ 1,
                /* normalTabCount= */ 0,
                /* incognitoTabCount= */ 0,
                /* isVisible= */ true,
                SupportedProfileType.REGULAR);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID, 1);
        writeCrashExitReasonToPrefs();

        // Initialize metadata and attempt to show the crash recovery dialog.
        mDelegate.initializeCrashRecoveryMetadata();
        boolean shown =
                mDelegate.maybeShowCrashRecoveryDialog(mModalDialogManagerSupplier, mHostActivity);

        // Verify the prompt is skipped, the live task is finished, and recoverability is cleared.
        assertFalse(shown);
        verifyNoInteractions(mModalDialogManager);
        AppTask liveTask1 = mPreRecoveryAppTasks.get(1);
        verify(liveTask1).finishAndRemoveTask();
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
        assertStateReset();
    }

    private void setupOtherCrashedWindows(
            int numNonVisibleWindows,
            int numDefaultDisplayWindows,
            int numNonDefaultDisplayWindows) {
        int start = 1;
        int end = numNonVisibleWindows + 1;
        for (int i = start; i < end; i++) {
            setupCrashedWindow(i, /* isVisible= */ false);
        }
        start = end;
        end = start + numDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            setupCrashedWindow(i, /* isVisible= */ true);
        }
        start = end;
        end = end + numNonDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            setupCrashedWindow(i, /* isVisible= */ true);
        }
    }

    private void setupCrashedWindow(
            int windowId,
            int normalTabCount,
            int incognitoTabCount,
            boolean isVisible,
            @SupportedProfileType int profileType) {
        mFakeTimeTestRule.advanceMillis(100);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
        ChromeMultiInstancePersistentStore.writeTabCount(
                windowId, normalTabCount, incognitoTabCount);
        ChromeMultiInstancePersistentStore.writeIsVisible(windowId, isVisible);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, true);
        ChromeMultiInstancePersistentStore.writeProfileType(windowId, profileType);
        mCrashedWindows.add(new CrashRecoveryWindowInfo(windowId, isVisible));
    }

    private void setupCrashedWindow(int windowId, boolean isVisible) {
        setupCrashedWindow(windowId, 1, 0, isVisible, SupportedProfileType.REGULAR);
    }

    private void setupPreRecoveryAppTasks(Integer... windowIds) {
        mPreRecoveryAppTasks = new ArrayList<>();
        for (int windowId : windowIds) {
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = windowId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            mPreRecoveryAppTasks.add(appTask);
            if (!ChromeMultiInstancePersistentStore.hasInstance(windowId)) {
                ChromeMultiInstancePersistentStore.writeLastAccessedTime(windowId);
            }
            ChromeMultiInstancePersistentStore.writeTaskId(windowId, windowId);
        }
        ActivityManager activityManager =
                (ActivityManager) mHostActivity.getSystemService(Context.ACTIVITY_SERVICE);
        Shadows.shadowOf(activityManager).setAppTasks(mPreRecoveryAppTasks);
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

    private void assertRestoredWindowIntents(int... expectedWindowIds) {
        var shadowActivity = Shadows.shadowOf(mHostActivity);
        // ShadowActivity#getNextStartedActivity() pops in LIFO order.
        for (int i = expectedWindowIds.length - 1; i >= 0; i--) {
            Intent intent = shadowActivity.getNextStartedActivity();
            assertNotNull(intent);
            assertEquals(
                    expectedWindowIds[i], intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
            assertEquals(
                    NewWindowAppSource.CRASH_RECOVERY,
                    intent.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        }
        assertNull(shadowActivity.getNextStartedActivity());
    }

    private void assertStateReset() {
        assertTrue(mDelegate.getNonVisibleWindowsForTesting().isEmpty());
        assertTrue(mDelegate.getVisibleWindowsForTesting().isEmpty());
        assertTrue(mDelegate.getWindowIdsPendingRestorationForTesting().isEmpty());
    }
}
