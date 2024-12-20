// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.Outcome;
import org.chromium.components.collaboration.Type;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.SharedDataPreview;

/** An interface to manage collaboration flow UI screens. */
@JNINamespace("collaboration")
public class CollaborationControllerDelegateImpl implements CollaborationControllerDelegate {
    /**
     * Prepare and wait for the UI to be ready to be shown.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void prepareFlowUI(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Show an error dialog.
     *
     * @param errorType The type of error.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showError(@Type int errorType, long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Cancel and hide the UI.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void cancel(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Show the sign-in and sync authentication screens.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showAuthenticationUi(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /** Notify that the primary account has been modified. */
    @CalledByNative
    void notifySignInAndSyncStatusChange() {}

    /**
     * Show the join dialog screen.
     *
     * @param token Group id and token secret of the current join request.
     * @param previewData Preview of shared data.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showJoinDialog(GroupToken token, SharedDataPreview previewData, long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Show the share dialog screen.
     *
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void showShareDialog(long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /**
     * Open and show the local tab group.
     *
     * @param collaborationId The collaboration id of the tab group to promote.
     * @param resultCallback The callback to notify the outcome of the UI screen.
     */
    @CalledByNative
    void promoteTabGroup(String collaborationId, long resultCallback) {
        CollaborationControllerDelegateImplJni.get()
                .runResultCallback(Outcome.FAILURE, resultCallback);
    }

    /** Focus and show the current flow screen. */
    @CalledByNative
    void promoteCurrentScreen() {}

    @NativeMethods
    interface Natives {
        void runResultCallback(int joutcome, long resultCallback);
    }
}
