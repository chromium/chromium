// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import com.google.android.gms.tasks.Task;
import com.google.android.play.core.appupdate.AppUpdateManager;

import org.chromium.build.annotations.NullMarked;

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
     * the flag if the call fails. The flag is cleared first because Play normally kills the process
     * before any listener runs. The failure listener is not activity-scoped: those detach at
     * onStop, which Play's install UI triggers.
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
        task.addOnFailureListener(e -> InAppUpdatePolicy.setUpdateDownloaded(true));
        return task;
    }
}
