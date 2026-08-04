// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.LastSessionExitType;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabbedStartupWindowPolicyDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.ON_STARTUP_WINDOW_POLICY,
    ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH
})
public class TabbedStartupWindowPolicyDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityManager mActivityManager;
    @Mock private ChromeTabbedActivity mTabbedActivity;

    private TabbedStartupWindowPolicyDelegate mDelegate;

    @Before
    public void setUp() {
        TabbedStartupWindowPolicyDelegate.setInstanceForTesting(null);
        ChromeMultiInstancePersistentStore.ensureInitialized();
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        DeviceInfo.setIsDesktopForTesting(true);
        doReturn(mActivityManager).when(mTabbedActivity).getSystemService(Context.ACTIVITY_SERVICE);
        doReturn(ContextUtils.getApplicationContext().getPackageName())
                .when(mTabbedActivity)
                .getPackageName();
        mDelegate = TabbedStartupWindowPolicyDelegate.getInstance();
    }

    @After
    public void tearDown() {
        ChromeMultiInstancePersistentStore.resetForTesting();
        TabbedStartupWindowPolicyDelegate.setInstanceForTesting(null);
    }

    @Test
    public void testMaybeRestoreWindowsAfterLaunch_quit_restoresWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity).startActivity(any());
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
        assertFalse(
                "isRecoverable should be cleared when restoring window on launch after quit.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeRestoreWindowsAfterLaunch_defaultExitType_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.DEFAULT);

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertTrue(
                "isRecoverable should remain true when exit type is default.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testMaybeRestoreWindowsAfterLaunch_featureDisabled_doesNotRestoreWindows() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeRestoreWindowsAfterLaunch_quit_aliveTaskNonMultiWindowMode_doesNotRestoreWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
        setupAppTasks(1);
        doReturn(false).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertTrue(
                "isRecoverable should remain true when task is alive in non-multiwindow mode.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeRestoreWindowsAfterLaunch_quit_aliveTaskMultiWindowMode_restoresWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
        List<AppTask> appTasks = setupAppTasks(1);
        doReturn(true).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(appTasks.get(0)).finishAndRemoveTask();
        verify(mTabbedActivity).startActivity(any());
        assertFalse(
                "isRecoverable should be cleared when restoring window in multi-window mode.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeRestoreWindowsAfterLaunch_quit_killedTask_restoresWindow() {
        // Setup.
        setupRecoverableInstances(LastSessionExitType.QUIT);
        // Do not add app task ID 1 so it is treated as killed/not alive.
        doReturn(false).when(mTabbedActivity).isInMultiWindowMode();

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity).startActivity(any());
        assertFalse(
                "isRecoverable should be cleared when restoring a killed task.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeRestoreWindowsAfterLaunch_quit_notRecoverable_doesNotRestoreWindow() {
        // Setup.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeIsRecoverable(1, false);
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(LastSessionExitType.QUIT);
        doReturn(0).when(mTabbedActivity).getWindowId();

        // Act.
        mDelegate.maybeRestoreWindowsAfterLaunch(mTabbedActivity);

        // Verify.
        verify(mTabbedActivity, never()).startActivity(any());
        assertFalse(
                "isRecoverable should remain false for unrecoverable window.",
                ChromeMultiInstancePersistentStore.readIsRecoverable(1));
    }

    @Test
    public void testMaybeSaveWindowStateOnSessionTermination_multipleActiveInstances_savesState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is updated.
        assertEquals(
                LastSessionExitType.QUIT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_singleActiveInstance_doesNotSaveState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    public void
            testMaybeSaveWindowStateOnSessionTermination_singleActiveInstance_closedByApp_savesState() {
        // Setup only 1 active instance.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP);

        // Verify exit type is updated even for single instance.
        assertEquals(
                LastSessionExitType.LAST_WINDOW_CLOSED_BY_APP,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testMaybeSaveWindowStateOnSessionTermination_featureDisabled_doesNotSaveState() {
        // Setup 2 active instances.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 2, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 2);

        // Act.
        mDelegate.maybeSaveWindowStateOnSessionTermination(LastSessionExitType.QUIT);

        // Verify exit type is not updated when feature is disabled.
        assertEquals(
                LastSessionExitType.DEFAULT,
                ChromeMultiInstancePersistentStore.readLastSessionExitType());
    }

    private void setupRecoverableInstances(@LastSessionExitType int exitType) {
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 0, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 0);
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 1, "https://www.google.com", /* tabCount= */ 1, /* taskId= */ 1);
        ChromeMultiInstancePersistentStore.writeLastSessionExitType(exitType);

        doReturn(0).when(mTabbedActivity).getWindowId();
    }

    private List<AppTask> setupAppTasks(Integer... taskIds) {
        List<AppTask> appTasks = new ArrayList<>();
        for (int taskId : taskIds) {
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = taskId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            appTasks.add(appTask);
        }
        doReturn(appTasks).when(mActivityManager).getAppTasks();
        return appTasks;
    }
}
