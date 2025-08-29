// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.PendingState;

/** A callback class used by native C++ code to notify or finish the background task. */
@NullMarked
public class DownloadBackgroundTaskCallback {
    // Callback to invoke to finish the background task.
    private final Callback<Boolean> mTaskFinishedCallback;

    public DownloadBackgroundTaskCallback(Callback<Boolean> taskFinishedCallback) {
        mTaskFinishedCallback = taskFinishedCallback;
    }

    /**
     * Called by native to notify that a download has started resuming. This call will send a
     * notification to Android as soon as the download resumption starts without waiting for network
     * in order to comply with Android's background process restrictions. Subsequently when the
     * download actually starts getting response from the network, this placeholder notification is
     * changed to include the actual download progress.
     *
     * @param item The OfflineItem that has started resuming.
     */
    @CalledByNative
    private void notify(OfflineItem item) {
        if (item.id == null) return;
        DownloadNotificationService.getInstance()
                .notifyDownloadPending(
                        item.id,
                        item.title,
                        OtrProfileId.deserializeWithoutVerify(item.otrProfileId),
                        item.allowMetered,
                        item.isTransient,
                        /* icon= */ null,
                        item.originalUrl,
                        /* shouldPromoteOrigin= */ false,
                        /* hasUserGesture= */ false,
                        PendingState.PENDING_NETWORK);
    }

    /**
     * Called by native to finish the background task.
     *
     * @param shouldReschedule Whether the task should be rescheduled.
     */
    @CalledByNative
    private void finishTask(boolean shouldReschedule) {
        mTaskFinishedCallback.onResult(shouldReschedule);
    }
}
