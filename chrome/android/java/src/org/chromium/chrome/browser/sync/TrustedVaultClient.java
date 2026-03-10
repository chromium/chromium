// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.VisibleForTesting;

/**
 * Backward-compatible shim.
 *
 * <p>TODO(crbug.com/40915967): Remove this once downstream implementation is migrated to
 * org.chromium.components.trusted_vault.TrustedVaultClient.
 */
@Deprecated
public class TrustedVaultClient extends org.chromium.components.trusted_vault.TrustedVaultClient {
    public interface Backend
            extends org.chromium.components.trusted_vault.TrustedVaultClient.Backend {}

    @VisibleForTesting
    public TrustedVaultClient(Backend backend) {
        super(backend);
    }
}
