// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.graphics.Rect;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ChromeTabbedActivity mHostActivity;

    private TabbedCrashRecoveryDelegate mDelegate;
    private SettableMonotonicObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private List<CrashRecoveryWindowInfo> mCrashedWindows;

    @Before
    public void setUp() {
        ChromeMultiInstancePersistentStore.ensureInitialized();
        mDelegate = TabbedCrashRecoveryDelegate.getInstance();
        mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
        mModalDialogManagerSupplier.set(mModalDialogManager);
        when(mHostActivity.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        when(mHostActivity.getWindowId()).thenReturn(HOST_WINDOW_ID);
        setupPreRecoveryAppTasks(HOST_WINDOW_ID);
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
    }

    @Test
    public void testInitiateCrashRecovery_singleWindow_skipsRecoveryPrompt() {
        // Setup.
        setupCrashedWindows(/* numNonVisibleWindows= */ 0, /* numVisibleWindows= */ 1);

        // Act.
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Verify.
        verifyNoInteractions(mModalDialogManager);
    }

    @Test
    public void testInitiateCrashRecovery_allOtherWindowsHaveLiveTasks_skipsRecoveryPrompt() {
        // Setup.
        setupCrashedWindows(/* numNonVisibleWindows= */ 1, /* numVisibleWindows= */ 2);
        setupPreRecoveryAppTasks(0, 1, 2);

        // Act.
        mDelegate.initiateCrashRecovery(
                mModalDialogManagerSupplier, mHostActivity, mCrashedWindows);

        // Verify.
        verifyNoInteractions(mModalDialogManager);
    }

    private void setupCrashedWindows(int numNonVisibleWindows, int numVisibleWindows) {
        mCrashedWindows = new ArrayList<>();
        // Include the host window in the visible window count.
        mCrashedWindows.add(
                new CrashRecoveryWindowInfo(HOST_WINDOW_ID, HOST_BOUNDS, /* isVisible= */ true));
        int testWindowWidth = 800;
        int testWindowHeight = 600;
        for (int i = 1; i < numNonVisibleWindows; i++) {
            mCrashedWindows.add(
                    new CrashRecoveryWindowInfo(i, /* bounds= */ null, /* isVisible= */ false));
        }
        for (int i = numNonVisibleWindows; i < numNonVisibleWindows + numVisibleWindows - 1; i++) {
            mCrashedWindows.add(
                    new CrashRecoveryWindowInfo(
                            i,
                            new Rect(
                                    i * 10,
                                    i * 10,
                                    i * 10 + testWindowWidth,
                                    i * 10 + testWindowHeight),
                            /* isVisible= */ true));
        }
    }

    private void setupPreRecoveryAppTasks(Integer... windowIds) {
        List<AppTask> tasks = new ArrayList<>();
        for (int windowId : windowIds) {
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = windowId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            tasks.add(appTask);
            ChromeMultiInstancePersistentStore.writeTaskId(windowId, windowId);
        }
        when(mActivityManager.getAppTasks()).thenReturn(tasks);
    }
}
