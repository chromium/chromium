// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
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
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
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
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabbedCrashRecoveryDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)
public class TabbedCrashRecoveryDelegateUnitTest {
    private static final int HOST_WINDOW_ID = 0;
    private static final Rect HOST_BOUNDS = new Rect(0, 0, 800, 600);
    private static final int TEST_WINDOW_WIDTH = 800;
    private static final int TEST_WINDOW_HEIGHT = 600;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ChromeTabbedActivity mHostActivity;

    private TabbedCrashRecoveryDelegate mDelegate;
    private SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private List<CrashRecoveryWindowInfo> mCrashedWindows;
    private List<AppTask> mPreRecoveryAppTasks;

    @Before
    public void setUp() {
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mDelegate = TabbedCrashRecoveryDelegate.getInstance();
        mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
        mModalDialogManagerSupplier.set(mModalDialogManager);
        when(mHostActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        ApplicationStatus.onStateChangeForTesting(mHostActivity, ActivityState.CREATED);
        when(mHostActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        when(mHostActivity.getWindowId()).thenReturn(HOST_WINDOW_ID);
        mCrashedWindows = new ArrayList<>();
        // Include the recovered host window in the list of crashed windows.
        mCrashedWindows.add(
                new CrashRecoveryWindowInfo(HOST_WINDOW_ID, HOST_BOUNDS, /* isVisible= */ true));
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
        TabbedCrashRecoveryDelegate.resetForTesting();
    }

    @Test
    public void testInitiateCrashRecovery_singleWindow_skipsRecoveryPrompt() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 0,
                /* numNonDefaultDisplayWindows= */ 0);
        var initWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.CrashRecoveryWindowCount", 1)
                        .build();

        // Act.
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Verify.
        verifyNoInteractions(mModalDialogManager);
        initWatcher.assertExpected();
    }

    @Test
    public void testInitiateCrashRecovery_allOtherWindowsHaveLiveTasks_skipsRecoveryPrompt() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(0, 1, 2);
        var initWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.MultiWindow.CrashRecoveryWindowCount", 3)
                        .build();

        // Act.
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Verify.
        verifyNoInteractions(mModalDialogManager);
        initWatcher.assertExpected();
    }

    @Test
    public void testRestoreWindows_restoresVisibleWindowsInNewTasks() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 1);
        setupPreRecoveryAppTasks(0);
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify.
        ArgumentCaptor<Intent> intentCaptor1 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor2 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);

        InOrder inOrderVerifier = inOrder(mHostActivity);

        // Verify: Window from default display is restored with cached bounds.
        inOrderVerifier
                .verify(mHostActivity)
                .startActivity(intentCaptor1.capture(), bundleCaptor.capture());
        Intent intent1 = intentCaptor1.getValue();
        assertNotNull(intent1);
        assertEquals(1, intent1.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent1.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        Bundle bundle = bundleCaptor.getValue();
        assertNotNull(bundle);
        // For windowId=1, setupOtherCrashedWindows assigns left=10, top=10,
        // right=10+TEST_WINDOW_WIDTH, bottom=10+TEST_WINDOW_HEIGHT.
        assertEquals(
                new Rect(10, 10, 10 + TEST_WINDOW_WIDTH, 10 + TEST_WINDOW_HEIGHT),
                ActivityOptions.fromBundle(bundle).getLaunchBounds());
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));

        // Verify: Window from non-default display is restored without launch bounds.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor2.capture(), eq(null));
        Intent intent2 = intentCaptor2.getValue();
        assertNotNull(intent2);
        assertEquals(2, intent2.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent2.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void testRestoreWindows_finishesOrphanedTask() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 0,
                /* numDefaultDisplayWindows= */ 2,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: windowId=2 has a task that sustained a crash.
        setupPreRecoveryAppTasks(0, 2);
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify.
        AppTask liveTask = mPreRecoveryAppTasks.get(1);
        verify(liveTask).finishAndRemoveTask();

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mHostActivity, times(2))
                .startActivity(intentCaptor.capture(), bundleCaptor.capture());

        List<Intent> intents = intentCaptor.getAllValues();
        assertNotNull(intents.get(0));
        assertEquals(1, intents.get(0).getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intents.get(0).getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertNotNull(intents.get(1));
        assertEquals(2, intents.get(1).getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intents.get(1).getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));

        List<Bundle> bundles = bundleCaptor.getAllValues();
        assertNotNull(bundles.get(0));
        // For windowId=1, setupOtherCrashedWindows assigns left=10, top=10,
        // right=10+TEST_WINDOW_WIDTH, bottom=10+TEST_WINDOW_HEIGHT.
        assertEquals(
                new Rect(10, 10, 10 + TEST_WINDOW_WIDTH, 10 + TEST_WINDOW_HEIGHT),
                ActivityOptions.fromBundle(bundles.get(0)).getLaunchBounds());
        assertNotNull(bundles.get(1));
        // For windowId=2, setupOtherCrashedWindows assigns left=20, top=20,
        // right=20+TEST_WINDOW_WIDTH, bottom=20+TEST_WINDOW_HEIGHT.
        assertEquals(
                new Rect(20, 20, 20 + TEST_WINDOW_WIDTH, 20 + TEST_WINDOW_HEIGHT),
                ActivityOptions.fromBundle(bundles.get(1)).getLaunchBounds());

        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void testRestoreWindows_restoresNonVisibleWindowInNewTask() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        setupPreRecoveryAppTasks(0);
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify.
        ArgumentCaptor<Intent> intentCaptor1 = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<Intent> intentCaptor2 = ArgumentCaptor.forClass(Intent.class);

        InOrder inOrderVerifier = inOrder(mHostActivity);

        // Verify: Non-visible window is restored without launch bounds bundle.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor1.capture());
        Intent intent1 = intentCaptor1.getValue();
        assertNotNull(intent1);
        assertEquals(1, intent1.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertEquals(
                NewWindowAppSource.CRASH_RECOVERY,
                intent1.getIntExtra(IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(1));

        // Verify: Visible window is restored with bundle.
        inOrderVerifier.verify(mHostActivity).startActivity(intentCaptor2.capture(), any());
        Intent intent2 = intentCaptor2.getValue();
        assertNotNull(intent2);
        assertEquals(2, intent2.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, -1));
        assertFalse(ChromeMultiInstancePersistentStore.readIsRecoverable(2));
    }

    @Test
    public void testRestoreWindows_skipsNonVisibleWindowWithLiveTask() {
        // Setup.
        setupOtherCrashedWindows(
                /* numNonVisibleWindows= */ 1,
                /* numDefaultDisplayWindows= */ 1,
                /* numNonDefaultDisplayWindows= */ 0);
        // Setup: Non-visible window (windowId=1) has a live task.
        setupPreRecoveryAppTasks(0, 1);
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);
        var expectedWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.MultiWindow.CrashRecoveryDuration")
                        .build();
        var userActionTester = new UserActionTester();

        // Act.
        mDelegate.restoreWindows(mHostActivity);

        // Verify: The live task for the non-visible window should not be finished.
        AppTask liveTask = mPreRecoveryAppTasks.get(1);
        verify(liveTask, never()).finishAndRemoveTask();

        // Verify: Only the visible window (windowId=2) should be started.
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mHostActivity).startActivity(intentCaptor.capture(), any());
        mDelegate.registerRecovery(2);
        // Verify: No single-arg startActivity was called for the non-visible window.
        verify(mHostActivity, never()).startActivity(any());

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
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);
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

    private void setupOtherCrashedWindows(
            int numNonVisibleWindows,
            int numDefaultDisplayWindows,
            int numNonDefaultDisplayWindows) {
        int start = 1;
        int end = numNonVisibleWindows + 1;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            mCrashedWindows.add(
                    new CrashRecoveryWindowInfo(i, /* bounds= */ null, /* isVisible= */ false));
        }
        start = end;
        end = start + numDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            mCrashedWindows.add(
                    new CrashRecoveryWindowInfo(
                            i,
                            new Rect(
                                    i * 10,
                                    i * 10,
                                    i * 10 + TEST_WINDOW_WIDTH,
                                    i * 10 + TEST_WINDOW_HEIGHT),
                            /* isVisible= */ true));
        }
        start = end;
        end = end + numNonDefaultDisplayWindows;
        for (int i = start; i < end; i++) {
            ChromeMultiInstancePersistentStore.writeIsRecoverable(i, true);
            // Non-default display windows are visible pre-crash but lack tracked bounds.
            mCrashedWindows.add(
                    new CrashRecoveryWindowInfo(i, /* bounds= */ null, /* isVisible= */ true));
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
            ChromeMultiInstancePersistentStore.writeTaskId(windowId, windowId);
        }
        when(mActivityManager.getAppTasks()).thenReturn(mPreRecoveryAppTasks);
    }
}
