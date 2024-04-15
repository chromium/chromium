// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;

import java.util.Objects;

/**
 * Used by the web sign-in flow to detect when the flow is completed or failed. Every instance of
 * this class should be explicitly destroyed using {@link #destroy()} to correctly release native
 * resources.
 */
@MainThread
public class WebSigninBridge {
    /** Listener to be notified about sign-in completion. */
    public interface Listener {
        /**
         * Sign-in completed successfully and the primary account is available in the cookie jar.
         */
        void onSigninSucceeded();

        /**
         * Sign-in process failed.
         * @param error Details about the error that occurred in the sign-in process.
         */
        void onSigninFailed(GoogleServiceAuthError error);
    }

    /** Factory to create WebSigninBridge object. */
    public static class Factory {
        /**
         * Creates a WebSigninBridge object.
         *
         * @param profile The profile to use for the sign-in.
         * @param account The primary account account used for the sign-in process.
         * @param listener The listener to be notified about sign-in completion.
         */
        public WebSigninBridge create(Profile profile, CoreAccountInfo account, Listener listener) {
            return new WebSigninBridge(profile, account, listener);
        }
    }

    private long mNativeWebSigninBridge;

    /**
     * Notifies the passed {@link Listener} when the sign-in process completes either successfully
     * or with an error. Successful completion means that the primary account is available in
     * cookies. Should be explicitly destroyed using {@link #destroy()} to release native resources.
     * @param account The primary account account used for the sign-in process.
     * @param listener The listener to be notified about sign-in completion.
     */
    private WebSigninBridge(Profile profile, CoreAccountInfo account, Listener listener) {
        Objects.requireNonNull(account);
        Objects.requireNonNull(listener);
        mNativeWebSigninBridge = WebSigninBridgeJni.get().create(profile, account, listener);
        assert mNativeWebSigninBridge != 0 : "Couldn't create native WebSigninBridge object!";
    }

    /** Releases native resources used by this class. */
    public void destroy() {
        WebSigninBridgeJni.get().destroy(mNativeWebSigninBridge);
        mNativeWebSigninBridge = 0;
    }

    @VisibleForTesting
    @CalledByNative
    static void onSigninSucceeded(Listener listener) {
        listener.onSigninSucceeded();
    }

    @VisibleForTesting
    @CalledByNative
    static void onSigninFailed(Listener listener, GoogleServiceAuthError error) {
        listener.onSigninFailed(error);
    }

    @NativeMethods
    interface Natives {
        long create(
                @JniType("Profile*") Profile profile, CoreAccountInfo account, Listener listener);

        void destroy(long webSigninBridgePtr);
    }
}
