// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.download.dialogs.DangerousDownloadDialog;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues dangerous download dialogs UI code and handles the communication to download native
 * backend.
 */
public class DangerousDownloadDialogBridge {
    private long mNativeDangerousDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeDangerousDownloadDialogBridge Pointer to the native object.
     */
    public DangerousDownloadDialogBridge(long nativeDangerousDownloadDialogBridge) {
        mNativeDangerousDownloadDialogBridge = nativeDangerousDownloadDialogBridge;
    }

    @CalledByNative
    public static DangerousDownloadDialogBridge create(long nativeDialog) {
        return new DangerousDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for dangerous download.
     * @param windowAndroid Window to show the dialog.
     * @param guid GUID of the download.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param iconId The icon resource for the warning dialog.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            String guid,
            String fileName,
            long totalBytes,
            int iconId) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(guid, windowAndroid);
            return;
        }

        new DangerousDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        fileName,
                        totalBytes,
                        iconId,
                        (accepted) -> {
                            if (accepted) {
                                onAccepted(guid);
                            } else {
                                onCancel(guid, windowAndroid);
                            }
                        });
    }

    @CalledByNative
    private void destroy() {
        mNativeDangerousDownloadDialogBridge = 0;
    }

    private void onAccepted(String guid) {
        DangerousDownloadDialogBridgeJni.get().accepted(mNativeDangerousDownloadDialogBridge, guid);
    }

    private void onCancel(String guid, WindowAndroid windowAndroid) {
        DangerousDownloadDialogBridgeJni.get()
                .cancelled(mNativeDangerousDownloadDialogBridge, guid);
        NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
    }

    @NativeMethods
    interface Natives {
        void accepted(long nativeDangerousDownloadDialogBridge, String guid);

        void cancelled(long nativeDangerousDownloadDialogBridge, String guid);
    }
}
