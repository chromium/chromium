// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.dialogs.InsecureDownloadDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues insecure download dialogs UI code and handles the communication to download native
 * backend.
 */
@NullMarked
public class InsecureDownloadDialogBridge {
    @JniType("::InsecureDownloadDialogBridge")
    interface NativeInsecureDownloadDialogBridge extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeInsecureDownloadDialogBridge>
            mNativeInsecureDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeInsecureDownloadDialogBridge Pointer to the native object.
     */
    public InsecureDownloadDialogBridge(
            JniRawPtr<NativeInsecureDownloadDialogBridge> nativeInsecureDownloadDialogBridge) {
        mNativeInsecureDownloadDialogBridge = nativeInsecureDownloadDialogBridge;
    }

    @CalledByNative
    private static InsecureDownloadDialogBridge create(
            JniRawPtr<NativeInsecureDownloadDialogBridge> nativeDialog) {
        return new InsecureDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for insecure download.
     *
     * @param windowAndroid Window to show the dialog.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param callbackId Native callback Id to invoke.
     */
    @CalledByNative
    private void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String fileName,
            long totalBytes,
            long callbackId) {
        Activity activity = windowAndroid.getActivity().get();
        if (!(activity instanceof ModalDialogManagerHolder)) {
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
        if (mNativeInsecureDownloadDialogBridge != null) {
            mNativeInsecureDownloadDialogBridge.release();
            mNativeInsecureDownloadDialogBridge = null;
        }
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        if (mNativeInsecureDownloadDialogBridge != null) {
            InsecureDownloadDialogBridgeJni.get()
                    .onConfirmed(mNativeInsecureDownloadDialogBridge, callbackId, accepted);
        }
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                JniPtr<NativeInsecureDownloadDialogBridge> self,
                long callbackId,
                boolean accepted);
    }
}
