// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omaha.OmahaBase.UpdateStatus;
import org.chromium.chrome.browser.omaha.OmahaService;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;

import java.lang.ref.WeakReference;

/**
 * Glue code for interactions between Safety check and Omaha on Android.
 *
 * This class is needed because {@link OmahaService} is in //chrome/android,
 * while Safety check is modularized in //chrome/browser. Once Omaha is
 * modularized as well, this class will not be needed anymore.
 */
public class SafetyCheckUpdatesDelegateImpl implements SafetyCheckUpdatesDelegate {
    private OmahaService mOmaha;

    /**
     * Creates a new instance of the glue class to be passed to {@link SafetyCheckSettingsFragment}.
     */
    public SafetyCheckUpdatesDelegateImpl() {
        mOmaha = OmahaService.getInstance();
    }

    /**
     * Converts Omaha's {@link UpdateStatus} into a
     * {@link SafetyCheckModel.Updates} enum value.
     * @param status Update status returned by Omaha.
     * @return A corresponding {@link SafetyCheckModel.Updates} value.
     */
    public static @UpdatesState int convertOmahaUpdateStatus(@UpdateStatus int status) {
        switch (status) {
            case UpdateStatus.UPDATED:
                return UpdatesState.UPDATED;
            case UpdateStatus.OUTDATED:
                return UpdatesState.OUTDATED;
            case UpdateStatus.OFFLINE:
                return UpdatesState.OFFLINE;
            case UpdateStatus.FAILED: // Intentional fall through.
            default:
                return UpdatesState.ERROR;
        }
    }

    /**
     * Asynchronously checks for updates and invokes the provided callback with
     * the result.
     * @param statusCallback A callback to invoke with the result. Takes an element of
     *                       {@link SafetyCheckProperties.UpdatesState} as an argument.
     */
    @Override
    public void checkForUpdates(WeakReference<Callback<Integer>> statusCallback) {
        PostTask.postTask(
                TaskTraits.USER_VISIBLE,
                () -> {
                    @UpdateStatus int status = mOmaha.checkForUpdates();
                    // Post the results back to the UI thread.
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                Callback<Integer> strongRef = statusCallback.get();
                                if (strongRef != null) {
                                    strongRef.onResult(convertOmahaUpdateStatus(status));
                                }
                            });
                });
    }
}
