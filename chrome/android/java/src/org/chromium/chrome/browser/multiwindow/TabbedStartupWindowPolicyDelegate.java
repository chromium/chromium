// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.ActivityManager.AppTask;
import android.content.Intent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.LastSessionExitType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;

import java.util.Map;
import java.util.Set;

/**
 * Delegate to manage startup window policies and relaunch session restoration for {@link
 * ChromeTabbedActivity} windows.
 */
@NullMarked
public class TabbedStartupWindowPolicyDelegate {
    private static @Nullable TabbedStartupWindowPolicyDelegate sInstance;

    private TabbedStartupWindowPolicyDelegate() {}

    /** Returns the singleton instance of {@link TabbedStartupWindowPolicyDelegate}. */
    public static TabbedStartupWindowPolicyDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedStartupWindowPolicyDelegate();
        }
        return sInstance;
    }

    /**
     * Persists tabbed window state on session termination.
     *
     * @param exitType The {@link LastSessionExitType} to write.
     */
    public void maybeSaveWindowStateOnSessionTermination(@LastSessionExitType int exitType) {
        if (!MultiWindowUtils.isNewStartupWindowPolicyEnabled()) {
            return;
        }

        // If we are terminating the Chrome session with fewer than 2 active ChromeTabbedActivity
        // windows, there is no need to persist the RELAUNCH session type that is used to restore
        // all active windows upon relaunch.
        if (exitType == LastSessionExitType.RELAUNCH
                && MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE) <= 1) {
            return;
        }

        ChromeMultiInstancePersistentStore.writeLastSessionExitType(exitType);
    }

    /* package */ void maybeRestoreWindowsAfterRelaunch(ChromeTabbedActivity activity) {
        if (!MultiWindowUtils.isNewStartupWindowPolicyEnabled()
                || !MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return;
        }

        if (ChromeMultiInstancePersistentStore.readLastSessionExitType()
                != LastSessionExitType.RELAUNCH) {
            return;
        }

        // Clear the non-default session type after it has been first processed during an app launch
        // to prevent it from being incorrectly used subsequently.
        ChromeMultiInstancePersistentStore.clearLastSessionExitType();

        int currentInstanceId = activity.getWindowId();
        Set<Integer> allIds = ChromeMultiInstancePersistentStore.readAllInstanceIds();
        Map<Integer, AppTask> appTasksById = MultiWindowUtils.getAppTasksById(activity);
        boolean isMultiWindowMode = activity.isInMultiWindowMode();
        boolean windowsRestored = false;
        for (int windowId : allIds) {
            if (windowId == currentInstanceId) {
                continue;
            }

            // If Chrome starts in a fullscreen window and a restorable window's task is still
            // alive after previous app termination, skip processing such a window because we want
            // the host window to be in the foreground during such launch anyway.
            if (!isMultiWindowMode && MultiWindowUtils.isTaskAlive(windowId, appTasksById)) {
                continue;
            }

            if (ChromeMultiInstancePersistentStore.readIsRecoverable(windowId)) {
                Intent intent =
                        MultiWindowUtils.createNewWindowIntent(
                                activity,
                                windowId,
                                /* preferNew= */ false,
                                /* openAdjacently= */ isMultiWindowMode,
                                NewWindowAppSource.RELAUNCH);
                if (intent != null) {
                    // Finish any existing live task for this instance before starting a new
                    // activity in multi-window mode, to avoid creating duplicate tasks and leaving
                    // the old task orphaned in non-multiwindow mode.
                    if (MultiWindowUtils.isTaskAlive(windowId, appTasksById)) {
                        int taskId = ChromeMultiInstancePersistentStore.readTaskId(windowId);
                        AppTask task = appTasksById.get(taskId);
                        if (task != null) {
                            task.finishAndRemoveTask();
                        }
                    }

                    // Reset recoverability of an instance before creating a new activity for it to
                    // avoid propagating stale state for a window to a future session if the
                    // activity creation fails during restoration in the current session.
                    ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, false);
                    activity.startActivity(intent);
                    windowsRestored = true;
                }
            }
        }
        if (windowsRestored) {
            ApiCompatibilityUtils.moveTaskToFront(activity, activity.getTaskId(), /* flags= */ 0);
        }
    }

    /* package */ static void setInstanceForTesting(
            @Nullable TabbedStartupWindowPolicyDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }
}
