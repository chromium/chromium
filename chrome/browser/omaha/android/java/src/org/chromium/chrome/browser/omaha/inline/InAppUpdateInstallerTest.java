// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import com.google.android.gms.tasks.TaskCompletionSource;
import com.google.android.play.core.appupdate.AppUpdateManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;

/** Unit tests for {@link InAppUpdateInstaller}. */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InAppUpdateInstallerTest {
    @Before
    public void setUp() {
        InAppUpdatePolicy.resetForTesting();
    }

    @After
    public void tearDown() {
        InAppUpdatePolicy.resetForTesting();
    }

    @Test
    public void testCompleteUpdate_clearsFlagEagerlyAndKeepsItClearedOnAsyncSuccess() {
        TaskCompletionSource<Void> source = completeUpdateWithPendingTask();
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());

        source.setResult(null);
        ShadowLooper.idleMainLooper();
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());
    }

    @Test
    public void testCompleteUpdate_clearsFlagEagerlyAndRestoresOnAsyncFailure() {
        TaskCompletionSource<Void> source = completeUpdateWithPendingTask();
        assertFalse(InAppUpdatePolicy.isUpdateDownloaded());

        source.setException(new RuntimeException("Async failure"));
        ShadowLooper.idleMainLooper();
        assertTrue(InAppUpdatePolicy.isUpdateDownloaded());
    }

    @Test
    public void testCompleteUpdate_restoresFlagAndRethrowsOnSyncFailure() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        AppUpdateManager manager = mock(AppUpdateManager.class);
        when(manager.completeUpdate()).thenThrow(new RuntimeException("Sync failure"));

        assertThrows(RuntimeException.class, () -> InAppUpdateInstaller.completeUpdate(manager));
        assertTrue(InAppUpdatePolicy.isUpdateDownloaded());
    }

    /** Sets the flag, then calls the installer with a completeUpdate() task that stays pending. */
    private static TaskCompletionSource<Void> completeUpdateWithPendingTask() {
        InAppUpdatePolicy.setUpdateDownloaded(true);
        AppUpdateManager manager = mock(AppUpdateManager.class);
        TaskCompletionSource<Void> source = new TaskCompletionSource<>();
        when(manager.completeUpdate()).thenReturn(source.getTask());
        InAppUpdateInstaller.completeUpdate(manager);
        return source;
    }
}
