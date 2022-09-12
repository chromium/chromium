// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.AlertDialog;
import android.widget.Toast;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;

/**
 * Glues dangerous download dialogs UI code and handles the communication to download native
 * backend.
 */
public class DangerousDownloadDialogBridge {
    private long mNativeDangerousDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     * @nativeDangerousDownloadDialogBridge Pointer to the native object.
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
    public void showDialog(WindowAndroid windowAndroid, String guid, String fileName,
            long totalBytes, int iconId, boolean isOffTheRecord) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(guid);
            return;
        }

        Toast.makeText(activity, "TODO showDangerousDialog", Toast.LENGTH_SHORT).show();

        AlertDialog dialog = new AlertDialog.Builder(activity)
                .setTitle("Download Dangerous File?")
                .setMessage("Are you sure to download this dangerous file? ")
                .setPositiveButton(org.chromium.chrome.browser.download.R.string.ok, (dialog13, which) -> {
                    DangerousDownloadDialogBridge.this.onAccepted(guid);
                    dialog13.dismiss();
                })
                .setNegativeButton(org.chromium.chrome.browser.download.R.string.cancel, (dialog12, which) -> {
                    DangerousDownloadDialogBridge.this.onCancel(guid);
                    dialog12.dismiss();
                })
                .setOnCancelListener(dialog1 -> DangerousDownloadDialogBridge.this.onCancel(guid))
                .create();
        dialog.show();
    }

    @CalledByNative
    public void showGlobalDialog(String guid, String fileName,
                           long totalBytes, int iconId, boolean isOffTheRecord) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            Toast.makeText(ContextUtils.getApplicationContext(),
                    "showGlobalDialog activity is null!", Toast.LENGTH_SHORT).show();
            return;
        }
        Toast.makeText(activity, "TODO showGlobalDialog", Toast.LENGTH_SHORT).show();

        AlertDialog dialog = new AlertDialog.Builder(activity)
                .setTitle("Download Dangerous File?")
                .setMessage("Are you sure to download this dangerous file? ")
                .setPositiveButton(org.chromium.chrome.browser.download.R.string.ok, (dialog13, which) -> {
                    DangerousDownloadDialogBridge.this.onAccepted(guid);
                    dialog13.dismiss();
                })
                .setNegativeButton(org.chromium.chrome.browser.download.R.string.cancel, (dialog12, which) -> {
                    DangerousDownloadDialogBridge.this.onCancel(guid);
                    dialog12.dismiss();
                })
                .setOnCancelListener(dialog1 -> DangerousDownloadDialogBridge.this.onCancel(guid))
                .create();
        dialog.show();
    }

    @CalledByNative
    private void destroy() {
        mNativeDangerousDownloadDialogBridge = 0;
    }

    private void onAccepted(String guid) {
        DangerousDownloadDialogBridgeJni.get().accepted(mNativeDangerousDownloadDialogBridge, guid);
    }

    private void onCancel(String guid) {
        DangerousDownloadDialogBridgeJni.get().cancelled(
                mNativeDangerousDownloadDialogBridge, guid);
    }

    @NativeMethods
    interface Natives {
        void accepted(long nativeDangerousDownloadDialogBridge, String guid);
        void cancelled(long nativeDangerousDownloadDialogBridge, String guid);
    }
}
