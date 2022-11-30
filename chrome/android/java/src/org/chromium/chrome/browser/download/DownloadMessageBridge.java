// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

public class DownloadMessageBridge {
    private long mNativeDownloadMessageBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     * @nativeDownloadMessageBridge Pointer to the native object.
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
                DownloadManagerService.getDownloadManagerService().getMessageUiController(
                        /*otrProfileID=*/null);
        messageUiController.showIncognitoDownloadMessage(
                (accepted) -> { onConfirmed(callbackId, accepted); });
    }

    @CalledByNative
    private void destroy() {
        mNativeDownloadMessageBridge = 0;
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        if (mNativeDownloadMessageBridge != 0) {
            DownloadMessageBridgeJni.get().onConfirmed(
                    mNativeDownloadMessageBridge, callbackId, accepted);
        }
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(long nativeDownloadMessageBridge, long callbackId, boolean accepted);
    }
}
