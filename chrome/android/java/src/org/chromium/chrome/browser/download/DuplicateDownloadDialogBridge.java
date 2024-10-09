// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues duplicate download dialogs UI code and handles the communication to download native
 * backend.
 */
public class DuplicateDownloadDialogBridge {
    private long mNativeDuplicateDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeDuplicateDownloadDialogBridge Pointer to the native object.
     */
    public DuplicateDownloadDialogBridge(long nativeDuplicateDownloadDialogBridge) {
        mNativeDuplicateDownloadDialogBridge = nativeDuplicateDownloadDialogBridge;
    }

    @CalledByNative
    public static DuplicateDownloadDialogBridge create(long nativeDialog) {
        return new DuplicateDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a dialog to confirm user wants to download a duplicate file.
     *
     * @param windowAndroid Window to show the dialog.
     * @param filePath Path of the download file.
     * @param pageUrl URL of the page, empty for file downloads.
     * @param totalBytes Total bytes of the file.
     * @param duplicateExists Whether a duplicate download is in progress.
     * @param otrProfileID Off the record profile ID.
     * @param callbackId Pointer to the native callback.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::string") String filePath,
            @JniType("std::string") String pageUrl,
            long totalBytes,
            boolean duplicateExists,
            OTRProfileID otrProfileID,
            long callbackId) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onConfirmed(callbackId, false);
            return;
        }
        new DuplicateDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        filePath,
                        pageUrl,
                        totalBytes,
                        duplicateExists,
                        otrProfileID,
                        (accepted) -> {
                            onConfirmed(callbackId, accepted);
                        });
    }

    @CalledByNative
    private void destroy() {
        mNativeDuplicateDownloadDialogBridge = 0;
    }

    /**
     * Called when user accepts the download
     *
     * @param callbackId Pointer to the native callback.
     */
    private void onConfirmed(long callbackId, boolean accepted) {
        DuplicateDownloadDialogBridgeJni.get()
                .onConfirmed(mNativeDuplicateDownloadDialogBridge, callbackId, accepted);
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                long nativeDuplicateDownloadDialogBridge, long callbackId, boolean accepted);
    }
}
