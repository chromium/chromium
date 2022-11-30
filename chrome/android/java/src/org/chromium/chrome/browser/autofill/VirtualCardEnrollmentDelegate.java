// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;

/**
 * Delegate that facilitates making native calls from the Android view for virtual card enrollment
 * flows.
 */
@JNINamespace("autofill")
public class VirtualCardEnrollmentDelegate {
    private long mNativeVirtualCardEnrollBubbleControllerImpl;

    /**
     * Creates an instance of a {@link VirtualCardEnrollmentDelegate}.
     *
     * @param nativeVirtualCardEnrollBubbleControllerImpl The pointer to the native controller
     *                                                    object for callbacks.
     */
    private VirtualCardEnrollmentDelegate(long nativeVirtualCardEnrollBubbleControllerImpl) {
        mNativeVirtualCardEnrollBubbleControllerImpl = nativeVirtualCardEnrollBubbleControllerImpl;
    }

    @CalledByNative
    private static VirtualCardEnrollmentDelegate create(
            long nativeVirtualCardEnrollBubbleControllerImpl) {
        return new VirtualCardEnrollmentDelegate(nativeVirtualCardEnrollBubbleControllerImpl);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeVirtualCardEnrollBubbleControllerImpl = 0;
    }

    /**
     * Callback when the user accepts the virtual card enrollment dialog.
     */
    public void onAccepted() {
        if (mNativeVirtualCardEnrollBubbleControllerImpl != 0) {
            VirtualCardEnrollmentDelegateJni.get().onAccepted(
                    mNativeVirtualCardEnrollBubbleControllerImpl);
        }
    }

    /**
     * Callback when the user declines the virtual card enrollment dialog.
     */
    public void onDeclined() {
        if (mNativeVirtualCardEnrollBubbleControllerImpl != 0) {
            VirtualCardEnrollmentDelegateJni.get().onDeclined(
                    mNativeVirtualCardEnrollBubbleControllerImpl);
        }
    }

    /**
     * Callback that is run when the virtual card enrollment dialog is dismissed either by user
     * interaction or by native controller.
     */
    public void onDismissed() {
        if (mNativeVirtualCardEnrollBubbleControllerImpl != 0) {
            VirtualCardEnrollmentDelegateJni.get().onDismissed(
                    mNativeVirtualCardEnrollBubbleControllerImpl);
        }
    }

    /**
     * Callback when a link is clicked in the virtual card enrollment dialog.
     * @param url                               URL of the link.
     * @param virtualCardEnrollmentLinkType     Type of link (Education text, Google TOS, or issuer
     *                                          TOS)
     */
    public void onLinkClicked(
            String url, @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType) {
        if (mNativeVirtualCardEnrollBubbleControllerImpl != 0) {
            VirtualCardEnrollmentDelegateJni.get().onLinkClicked(
                    mNativeVirtualCardEnrollBubbleControllerImpl, url,
                    virtualCardEnrollmentLinkType);
        }
    }

    @NativeMethods
    interface Natives {
        void onAccepted(long nativeVirtualCardEnrollBubbleControllerImpl);
        void onDeclined(long nativeVirtualCardEnrollBubbleControllerImpl);
        void onDismissed(long nativeVirtualCardEnrollBubbleControllerImpl);
        void onLinkClicked(long nativeVirtualCardEnrollBubbleControllerImpl, String url,
                @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType);
    }
}
