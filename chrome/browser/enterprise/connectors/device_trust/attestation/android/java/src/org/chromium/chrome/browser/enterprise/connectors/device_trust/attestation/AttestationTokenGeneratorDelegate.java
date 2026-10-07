// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.connectors.device_trust.attestation;

import org.chromium.build.annotations.NullMarked;

/**
 * Delegate interface for generating attestation tokens. Downstream implementations provide
 * integration with Google Play Services.
 *
 * <p>Methods are called from native code on a background thread pool worker and may block, but
 * implementations must return within a bounded time, since the worker is held until the call
 * returns.
 */
@NullMarked
public interface AttestationTokenGeneratorDelegate {
    /**
     * Generates an attestation token using the provided content binding hash. May block, but must
     * return within a bounded time.
     *
     * @param contentBinding The content binding bytes.
     * @return The result containing the token or an error message.
     */
    AttestationTokenResult generateToken(byte[] contentBinding);

    /** Pre-warms the integrity token cache. May block, but must return within a bounded time. */
    void preWarmCache();
}
