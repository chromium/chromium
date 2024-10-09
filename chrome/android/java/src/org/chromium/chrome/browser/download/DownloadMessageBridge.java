// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

public class DownloadMessageBridge {
    private long mNativeDownloadMessageBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeDownloadMessageBridge Pointer to the native object.
     */
    public DownloadMessageBridge(long nativeDownloadMessageBridge) {
        mNativeDownloadMessageBridge = nativeDownloadMessageBridge;
    }

    @CalledByNative
    public static DownloadMessageBridge create(long nativeDialog) {
        return new DownloadMessageBridge(nativeDialog);
    }

    @CalledByNative
    public void showIncognitoDownloadMessage(long callbackId) {
        DownloadMessageUiController messageUiController =
                DownloadManagerService.getDownloadManagerService()
                        .getMessageUiController(/* otrProfileID= */ null);
        messageUiController.showIncognitoDownloadMessage(
                (accepted) -> {
                    onConfirmed(callbackId, accepted);
                });
    }

    @CalledByNative
    public void showUnsupportedDownloadMessage(WindowAndroid window) {
        SnackbarManager snackbarManager = SnackbarManagerProvider.from(window);
        if (snackbarManager == null) return;

        Context context = window.getContext().get();
        Snackbar snackbar =
                Snackbar.make(
                        context.getString(R.string.download_file_type_not_supported),
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_AUTO_LOGIN);
        snackbar.setAction(context.getString(R.string.ok), null);
        snackbar.setSingleLine(false);
        snackbarManager.showSnackbar(snackbar);
    }

    @CalledByNative
    private void destroy() {
        mNativeDownloadMessageBridge = 0;
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        if (mNativeDownloadMessageBridge != 0) {
            DownloadMessageBridgeJni.get()
                    .onConfirmed(mNativeDownloadMessageBridge, callbackId, accepted);
        }
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(long nativeDownloadMessageBridge, long callbackId, boolean accepted);
    }
}
