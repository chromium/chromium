// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.download.dialogs.InsecureDownloadDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues insecure download dialogs UI code and handles the communication to download native
 * backend.
 */
public class InsecureDownloadDialogBridge {
    private long mNativeInsecureDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     * @param nativeInsecureDownloadDialogBridge Pointer to the native object.
     */
    public InsecureDownloadDialogBridge(long nativeInsecureDownloadDialogBridge) {
        mNativeInsecureDownloadDialogBridge = nativeInsecureDownloadDialogBridge;
    }

    @CalledByNative
    private static InsecureDownloadDialogBridge create(long nativeDialog) {
        return new InsecureDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for insecure download.
     * @param windowAndroid Window to show the dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param callbackId Native callback Id to invoke.
     */
    @CalledByNative
    private void showDialog(
            WindowAndroid windowAndroid, String fileName, long totalBytes, long callbackId) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onConfirmed(callbackId, false);
            return;
        }

        new InsecureDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        fileName,
                        totalBytes,
                        (accepted) -> {
                            onConfirmed(callbackId, accepted);
                        });
    }

    @CalledByNative
    private void destroy() {
        mNativeInsecureDownloadDialogBridge = 0;
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        InsecureDownloadDialogBridgeJni.get()
                .onConfirmed(mNativeInsecureDownloadDialogBridge, callbackId, accepted);
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                long nativeInsecureDownloadDialogBridge, long callbackId, boolean accepted);
    }
}
