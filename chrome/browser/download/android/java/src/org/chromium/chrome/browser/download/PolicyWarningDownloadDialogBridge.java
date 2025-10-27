// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.dialogs.PolicyWarningDownloadDialog;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Dialog for warning the user about a download that has sensitive content, based on enterprise
 * policies.
 */
@NullMarked
public class PolicyWarningDownloadDialogBridge {
    private long mNativePolicyWarningDownloadDialogBridge;

    private PolicyWarningDownloadDialogBridge(long nativePolicyWarningDownloadDialogBridge) {
        mNativePolicyWarningDownloadDialogBridge = nativePolicyWarningDownloadDialogBridge;
    }

    @CalledByNative
    private static PolicyWarningDownloadDialogBridge create(long nativeDialog) {
        return new PolicyWarningDownloadDialogBridge(nativeDialog);
    }

    @CalledByNative
    private void destroy() {
        mNativePolicyWarningDownloadDialogBridge = 0;
    }

    @CalledByNative
    private void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::string") String guid,
            @JniType("std::u16string") String fileName) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(guid, windowAndroid);
            return;
        }

        new PolicyWarningDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        fileName,
                        (accepted) -> {
                            if (accepted) {
                                onAccepted(guid);
                            } else {
                                onCancel(guid, windowAndroid);
                            }
                        });
    }

    private void onAccepted(String guid) {
        PolicyWarningDownloadDialogBridgeJni.get()
                .accepted(mNativePolicyWarningDownloadDialogBridge, guid);
    }

    private void onCancel(String guid, WindowAndroid windowAndroid) {
        PolicyWarningDownloadDialogBridgeJni.get()
                .cancelled(mNativePolicyWarningDownloadDialogBridge, guid);
        NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
    }

    @NativeMethods
    interface Natives {
        void accepted(
                long nativePolicyWarningDownloadDialogBridge, @JniType("std::string") String guid);

        void cancelled(
                long nativePolicyWarningDownloadDialogBridge, @JniType("std::string") String guid);
    }
}
