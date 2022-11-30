// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.dialogs.MixedContentDownloadDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues mixed-content download dialogs UI code and handles the communication to download native
 * backend.
 */
public class MixedContentDownloadDialogBridge {
    private long mNativeMixedContentDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     * @param nativeMixedContentDownloadDialogBridge Pointer to the native object.
     */
    public MixedContentDownloadDialogBridge(long nativeMixedContentDownloadDialogBridge) {
        mNativeMixedContentDownloadDialogBridge = nativeMixedContentDownloadDialogBridge;
    }

    @CalledByNative
    private static MixedContentDownloadDialogBridge create(long nativeDialog) {
        return new MixedContentDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for mixed-content download.
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

        new MixedContentDownloadDialog().show(activity,
                ((ModalDialogManagerHolder) activity).getModalDialogManager(), fileName, totalBytes,
                (accepted) -> { onConfirmed(callbackId, accepted); });
    }

    @CalledByNative
    private void destroy() {
        mNativeMixedContentDownloadDialogBridge = 0;
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        MixedContentDownloadDialogBridgeJni.get().onConfirmed(
                mNativeMixedContentDownloadDialogBridge, callbackId, accepted);
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                long nativeMixedContentDownloadDialogBridge, long callbackId, boolean accepted);
    }
}
