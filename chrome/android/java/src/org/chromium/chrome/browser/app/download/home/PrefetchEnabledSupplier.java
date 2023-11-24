// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.download.OfflineContentAvailabilityStatusProvider;

/**
 * Helper class to determine whether or not the prefetch setting is enabled for Chrome.
 * This class does not require an explicit destroy call, but needs all observers to be
 * unregistered for full clean up.
 */
class PrefetchEnabledSupplier implements Supplier<Boolean> {
    // Supplier implementation.
    @Override
    public Boolean get() {
        return OfflineContentAvailabilityStatusProvider.getInstance().isSuggestedContentAvailable();
    }
}
