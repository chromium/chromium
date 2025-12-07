// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.extensions.browser.SupervisedExtensionApprovalResult;
import org.chromium.ui.base.WindowAndroid;

/** Requests approval from a parent of a supervised user to install a chrome extension. */
@NullMarked
class ExtensionParentApproval {
    /**
     * Whether or not local approval is supported.
     *
     * <p>This method should be called before {@link requestExtensionApproval()}.
     */
    @CalledByNative
    private static boolean isExtensionApprovalSupported() {
        return ParentAuthDelegateProvider.getInstance() != null;
    }

    /**
     * Request approval from a parent for installing a chrome extension.
     *
     * <p>This method handles displaying relevant UI, and when complete calls the provided callback
     * with the result. It should only be called after {@link isExtensionApprovalSupported} has
     * returned true (it will perform a no-op if extension approvals are unsupported).
     *
     * @param windowAndroid The window to which the approval UI should be attached.
     */
    @CalledByNative
    private static void requestExtensionApproval(WindowAndroid windowAndroid) {
        ParentAuthDelegate delegate = ParentAuthDelegateProvider.getInstance();
        assert delegate != null;
        delegate.requestExtensionAuth(
                windowAndroid,
                (success) -> {
                    onParentAuthComplete(success);
                });
    }

    private static void onParentAuthComplete(boolean success) {
        if (!success) {
            ExtensionParentApprovalJni.get()
                    .onCompletion(SupervisedExtensionApprovalResult.CANCELED);
            return;
        }

        ExtensionParentApprovalJni.get().onCompletion(SupervisedExtensionApprovalResult.APPROVED);
    }

    @NativeMethods
    interface Natives {
        void onCompletion(int resultValue);
    }
}
