// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.blink.mojom.PublicKeyCredentialCreationOptions;
import org.chromium.blink.mojom.PublicKeyCredentialRequestOptions;
import org.chromium.components.webauthn.AuthenticatorImpl;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/**
 * Acts as a bridge from InternalAuthenticator declared in
 * //chrome/browser/autofill/android/internal_authenticator_android.h to AuthenticatorImpl.
 *
 * The origin associated with requests on InternalAuthenticator should be set by calling
 * setEffectiveOrigin() first.
 */
public class InternalAuthenticator {
    private long mNativeInternalAuthenticatorAndroid;
    private final AuthenticatorImpl mAuthenticator;

    private InternalAuthenticator(
            long nativeInternalAuthenticatorAndroid, RenderFrameHost renderFrameHost) {
        mNativeInternalAuthenticatorAndroid = nativeInternalAuthenticatorAndroid;
        mAuthenticator = new AuthenticatorImpl(renderFrameHost);
    }

    @CalledByNative
    public static InternalAuthenticator create(
            long nativeInternalAuthenticatorAndroid, RenderFrameHost renderFrameHost) {
        return new InternalAuthenticator(nativeInternalAuthenticatorAndroid, renderFrameHost);
    }

    @CalledByNative
    public void clearNativePtr() {
        mNativeInternalAuthenticatorAndroid = 0;
    }

    @CalledByNative
    public void setEffectiveOrigin(Origin origin) {
        mAuthenticator.setEffectiveOrigin(origin);
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process.
     */
    @CalledByNative
    public void makeCredential(ByteBuffer optionsByteBuffer) {
        mAuthenticator.makeCredential(
                PublicKeyCredentialCreationOptions.deserialize(optionsByteBuffer),
                (status, response) -> {
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get().invokeMakeCredentialResponse(
                                mNativeInternalAuthenticatorAndroid, status.intValue(),
                                response == null ? null : response.serialize());
                    }
                });
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process.
     */
    @CalledByNative
    public void getAssertion(ByteBuffer optionsByteBuffer) {
        mAuthenticator.getAssertion(
                PublicKeyCredentialRequestOptions.deserialize(optionsByteBuffer),
                (status, response) -> {
                    if (mNativeInternalAuthenticatorAndroid != 0) {
                        InternalAuthenticatorJni.get().invokeGetAssertionResponse(
                                mNativeInternalAuthenticatorAndroid, status.intValue(),
                                response == null ? null : response.serialize());
                    }
                });
    }

    /**
     * Called by InternalAuthenticatorAndroid, which facilitates WebAuthn for processes that
     * originate from the browser process. The response will be passed through
     * |invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse()|.  This is exclusively called
     * by Payments Autofill, and because Payments servers only accept security keys for Android P
     * and above, this returns false if the build version is O or below. Otherwise, the return value
     * is determined by the AuthenticatorImpl implementation.
     */
    @CalledByNative
    public void isUserVerifyingPlatformAuthenticatorAvailable() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            if (mNativeInternalAuthenticatorAndroid != 0) {
                InternalAuthenticatorJni.get()
                        .invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                mNativeInternalAuthenticatorAndroid, false);
            }
            return;
        }

        mAuthenticator.isUserVerifyingPlatformAuthenticatorAvailable((isUVPAA) -> {
            if (mNativeInternalAuthenticatorAndroid != 0) {
                InternalAuthenticatorJni.get()
                        .invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                                mNativeInternalAuthenticatorAndroid, isUVPAA);
            }
        });
    }

    @CalledByNative
    public void cancel() {
        mAuthenticator.cancel();
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        void invokeMakeCredentialResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer);
        void invokeGetAssertionResponse(
                long nativeInternalAuthenticatorAndroid, int status, ByteBuffer byteBuffer);
        void invokeIsUserVerifyingPlatformAuthenticatorAvailableResponse(
                long nativeInternalAuthenticatorAndroid, boolean isUVPAA);
    }
}
