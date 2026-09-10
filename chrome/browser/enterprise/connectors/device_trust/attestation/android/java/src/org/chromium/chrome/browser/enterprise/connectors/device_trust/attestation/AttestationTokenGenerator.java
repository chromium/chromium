// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

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
    public static AttestationTokenResult generateToken(byte[] contentBinding) {
        return sDelegate.generateToken(contentBinding);
    }

    /** Triggers pre-warming of the integrity token cache asynchronously. */
    public static void preWarmCache() {
        sDelegate.preWarmCache();
    }

    public static void setDelegateForTesting(@Nullable AttestationTokenGeneratorDelegate delegate) {
        var previousDelegate = sDelegate;
        sDelegate = delegate != null ? delegate : createDelegate();
        ResettersForTesting.register(() -> sDelegate = previousDelegate);
    }
}
