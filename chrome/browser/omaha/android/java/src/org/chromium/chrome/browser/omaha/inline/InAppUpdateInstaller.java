// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import com.google.android.gms.tasks.Task;
import com.google.android.play.core.appupdate.AppUpdateManager;
import com.google.android.play.core.install.InstallException;
import com.google.android.play.core.install.model.InstallErrorCode;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Hands a downloaded in-app update to Play for installation and keeps the persisted downloaded flag
 * consistent with that request. Plain Java with no native dependencies, so it is safe to use from
 * the background task's cold process as well as from the activity-bound controller.
 */
@NullMarked
final class InAppUpdateInstaller {
    private InAppUpdateInstaller() {}

    /**
     * Clears the downloaded flag, then calls {@link AppUpdateManager#completeUpdate()}, restoring
     * the flag if the call fails while Play may still hold the download. The flag is cleared first
     * because Play normally kills the process before any listener runs. The failure listener is not
     * activity-scoped: those detach at onStop, which Play's install UI triggers.
     *
     * <p>The flag stays cleared when Play reports {@link
     * InstallErrorCode#ERROR_DOWNLOAD_NOT_PRESENT}. Play then has no downloaded update to install,
     * so a retry cannot succeed and a restored flag would keep the background task being
     * rescheduled.
     */
    static Task<Void> completeUpdate(AppUpdateManager manager) {
        InAppUpdatePolicy.setUpdateDownloaded(false);
        Task<Void> task;
        try {
            task = manager.completeUpdate();
        } catch (RuntimeException e) {
            InAppUpdatePolicy.setUpdateDownloaded(true);
            throw e;
        }
        task.addOnFailureListener(
                e -> {
                    if (!isDownloadNotPresent(e)) {
                        InAppUpdatePolicy.setUpdateDownloaded(true);
                    }
                });
        return task;
    }

    /** Returns whether {@code e} is Play reporting that it holds no downloaded update. */
    static boolean isDownloadNotPresent(@Nullable Exception e) {
        return e instanceof InstallException installException
                && installException.getErrorCode() == InstallErrorCode.ERROR_DOWNLOAD_NOT_PRESENT;
    }
}
