// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.content.Intent;

import androidx.annotation.CallSuper;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;

import java.util.HashSet;
import java.util.Set;

/**
 * Shared base delegate for managing startup window restoration, crash recovery state, and task
 * cleanup across tabbed windows.
 */
@NullMarked
/* package */ abstract class BaseTabbedStartupDelegate {
    /* Tracks the IDs of windows for which new activity creation has been initiated and that are
    awaiting completion. */
    private final Set<Integer> mWindowIdsPendingRestoration = new HashSet<>();
    /* Timestamp (in elapsedRealtimeMillis) when the first window activity launch in a restoration
    batch is initiated; used to compute the total restoration duration once all pending windows
    have been created. */
    private long mRestorationStartTime;

    protected BaseTabbedStartupDelegate() {}

    /**
     * Attempts to restore the specified window by starting a new activity for it, unless
     * restoration should be skipped (e.g. when a live task already exists in non-multi-window
     * mode).
     *
     * @param hostActivity The currently running host activity initiating the restoration.
     * @param windowId The ID of the window to restore.
     * @param task The existing {@link AppTask} associated with the window, or {@code null} if no
     *     live task exists.
     * @param source The {@link NewWindowAppSource} indicating the restoration trigger.
     * @return {@code true} if an activity launch was initiated for the window; {@code false} if
     *     restoration was skipped.
     */
    protected boolean restoreWindow(
            Activity hostActivity,
            int windowId,
            @Nullable AppTask task,
            @NewWindowAppSource int source) {
        // Reset recoverability of an instance before attempting restoration to avoid propagating
        // stale state for a window to a future session if the activity creation fails during
        // restoration in the current session, or if a skipped live task with a destroyed activity
        // is later swiped away from Recents.
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);

        boolean isInMultiWindowMode = hostActivity.isInMultiWindowMode();
        if (task != null) {
            if (!isInMultiWindowMode) {
                // Skip launching a new activity in non-multi-window mode since at most one window
                // will be visible and existing live tasks can remain in the background.
                return false;
            }
            // Force-remove the live task before starting a new one, as moveTaskToFront() fails
            // when a task's activity was previously destroyed (e.g. after a crash).
            task.finishAndRemoveTask();
        }
        // Lazily record the start time upon the first actual activity launch in the restoration
        // batch.
        if (mRestorationStartTime == 0) {
            mRestorationStartTime = TimeUtils.elapsedRealtimeMillis();
        }
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        hostActivity,
                        windowId,
                        /* preferNew= */ false,
                        isInMultiWindowMode,
                        source);
        hostActivity.startActivity(intent);
        mWindowIdsPendingRestoration.add(windowId);
        return true;
    }

    /**
     * Cleans up the recoverability state and associated task for the specified window.
     *
     * @param windowId The ID of the window to clean up.
     * @param task The {@link AppTask} to finish and remove, or {@code null} if task cleanup is not
     *     required.
     */
    protected void cleanUpWindow(int windowId, @Nullable AppTask task) {
        ChromeMultiInstancePersistentStore.writeIsRecoverable(windowId, /* isRecoverable= */ false);
        if (task != null) {
            task.finishAndRemoveTask();
        }
    }

    /**
     * Records that a window pending restoration has been created, and invokes {@link
     * #onAllWindowsRestored(long)} once all pending windows have been restored.
     *
     * @param windowId The ID of the window to remove from the pending restoration set.
     */
    protected void registerRestoration(int windowId) {
        boolean updated = mWindowIdsPendingRestoration.remove(windowId);
        if (updated && mWindowIdsPendingRestoration.isEmpty()) {
            long duration = TimeUtils.elapsedRealtimeMillis() - mRestorationStartTime;
            mRestorationStartTime = 0;
            onAllWindowsRestored(duration);
        }
    }

    /**
     * Invoked when all windows in {@link #mWindowIdsPendingRestoration} have completed restoration.
     *
     * @param durationMillis The elapsed time in milliseconds since restoration started.
     */
    protected abstract void onAllWindowsRestored(long durationMillis);

    /** Resets the delegate's in-memory startup and restoration tracking state. */
    @CallSuper
    protected void resetState() {
        mWindowIdsPendingRestoration.clear();
        mRestorationStartTime = 0;
    }

    /* package */ Set<Integer> getWindowIdsPendingRestorationForTesting() {
        return mWindowIdsPendingRestoration;
    }
}
