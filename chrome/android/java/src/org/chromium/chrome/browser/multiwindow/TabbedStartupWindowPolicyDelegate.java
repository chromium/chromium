// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.ActivityManager.AppTask;
import android.content.Intent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.LastSessionExitType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefService;

import java.util.Map;
import java.util.Set;

/**
 * Delegate to manage startup window policies and relaunch session restoration for {@link
 * ChromeTabbedActivity} windows.
 */
@NullMarked
public class TabbedStartupWindowPolicyDelegate {
    private static @Nullable TabbedStartupWindowPolicyDelegate sInstance;

    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable PrefService mPrefService;

    private TabbedStartupWindowPolicyDelegate() {}

    /** Returns the singleton instance of {@link TabbedStartupWindowPolicyDelegate}. */
    public static TabbedStartupWindowPolicyDelegate getInstance() {
        if (sInstance == null) {
            sInstance = new TabbedStartupWindowPolicyDelegate();
        }
        return sInstance;
    }

    /**
     * Initializes the delegate with native preferences once native is ready. This method is
     * idempotent and can be safely called multiple times across activity lifecycles.
     *
     * @param prefService The {@link PrefService} to observe.
     */
    public void initializeWithNative(PrefService prefService) {
        if (!MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled()) return;
        // Early return if already initialized to ensure idempotency across multiple activities.
        if (mPrefChangeRegistrar != null) return;
        mPrefService = prefService;
        mPrefChangeRegistrar = new PrefChangeRegistrar(prefService);
        mPrefChangeRegistrar.addObserver(
                Pref.RESTORE_ON_STARTUP, this::onRestoreOnStartupPrefChanged);
        onRestoreOnStartupPrefChanged();
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
        // windows, there is no need to persist the QUIT session type that is used to restore
        // all active windows upon next launch.
        if (exitType == LastSessionExitType.QUIT
                && MultiWindowUtils.getInstanceCount(PersistedInstanceType.ACTIVE) <= 1) {
            return;
        }

        ChromeMultiInstancePersistentStore.writeLastSessionExitType(exitType);
    }

    /* package */ int getCachedStartupPolicy() {
        if (!MultiWindowUtils.isRestoreOnStartupPrefSyncEnabled()) return 0;
        return ChromeMultiInstancePersistentStore.readRestoreOnStartupPrefValue();
    }

    /* package */ void maybeRestoreWindowsAfterLaunch(ChromeTabbedActivity activity) {
        if (!MultiWindowUtils.isNewStartupWindowPolicyEnabled()
                || !MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            return;
        }

        if (ChromeMultiInstancePersistentStore.readLastSessionExitType()
                != LastSessionExitType.QUIT) {
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

    @VisibleForTesting
    /* package */ void destroy() {
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        mPrefService = null;
    }

    private void onRestoreOnStartupPrefChanged() {
        int type = assertNonNull(mPrefService).getInteger(Pref.RESTORE_ON_STARTUP);
        ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(type);
    }

    /* package */ static void setInstanceForTesting(
            @Nullable TabbedStartupWindowPolicyDelegate delegate) {
        sInstance = delegate;
        ResettersForTesting.register(() -> sInstance = null);
    }
}
