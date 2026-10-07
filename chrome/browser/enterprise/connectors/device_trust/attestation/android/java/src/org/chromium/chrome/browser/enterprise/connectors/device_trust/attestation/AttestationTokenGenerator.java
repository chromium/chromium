// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Entry point for attestation token generation on Android. Delegates to a downstream implementation
 * if available, or falls back to a default delegate.
 */
@NullMarked
public class AttestationTokenGenerator {
    private static final String TAG = "DeviceTrust";

    private static AttestationTokenGeneratorDelegate sDelegate = createDelegate();

    private AttestationTokenGenerator() {}

    private static AttestationTokenGeneratorDelegate createDelegate() {
        AttestationTokenGeneratorDelegate delegate =
                ServiceLoaderUtil.maybeCreate(AttestationTokenGeneratorDelegate.class);
        return delegate != null ? delegate : new AttestationTokenGeneratorDefaultDelegate();
    }

    /**
     * Generates an attestation token using the provided content binding hash.
     *
     * @param contentBinding The content binding bytes.
     * @return The result of the token generation.
     */
    @CalledByNative
    public static AttestationTokenResult generateToken(byte[] contentBinding) {
        try {
            return sDelegate.generateToken(contentBinding);
        } catch (RuntimeException e) {
            return new AttestationTokenResult(null, e.toString());
        }
    }

    /** Triggers pre-warming of the integrity token cache. */
    @CalledByNative
    public static void preWarmCache() {
        try {
            sDelegate.preWarmCache();
        } catch (RuntimeException e) {
            Log.e(TAG, "Pre-warming the attestation token cache failed", e);
        }
    }

    public static void setDelegateForTesting(@Nullable AttestationTokenGeneratorDelegate delegate) {
        var previousDelegate = sDelegate;
        sDelegate = delegate != null ? delegate : createDelegate();
        ResettersForTesting.register(() -> sDelegate = previousDelegate);
    }

    /**
     * Replaces the current delegate and returns the previous one, without registering a resetter.
     * Used by native unit tests of the JNI bridge, which do not run {@link ResettersForTesting} and
     * are responsible for swapping the previous delegate back in.
     *
     * @param delegate The delegate to install.
     * @return The previously installed delegate.
     */
    @CalledByNativeForTesting
    static AttestationTokenGeneratorDelegate swapDelegateForTesting(
            AttestationTokenGeneratorDelegate delegate) {
        var previousDelegate = sDelegate;
        sDelegate = delegate;
        return previousDelegate;
    }

    /**
     * Creates a fake delegate whose generateToken() returns the given result. Used by native unit
     * tests of the JNI bridge.
     *
     * @param token The token to return, may be null.
     * @param errorMessage The error message to return, may be null.
     * @param returnNullResult If true, generateToken() returns null instead of a result object,
     *     violating the delegate contract to exercise the native defensive check.
     * @return The fake delegate.
     */
    @CalledByNativeForTesting
    static AttestationTokenGeneratorDelegate createFakeDelegateForTesting(
            byte @Nullable [] token, @Nullable String errorMessage, boolean returnNullResult) {
        return new AttestationTokenGeneratorDelegate() {
            // Returning null intentionally violates the @NullMarked contract.
            @SuppressWarnings("NullAway")
            @Override
            public AttestationTokenResult generateToken(byte[] contentBinding) {
                if (returnNullResult) {
                    return null;
                }
                return new AttestationTokenResult(token, errorMessage);
            }

            @Override
            public void preWarmCache() {}
        };
    }
}
