// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory for creating {@link PinnedTabClosureManager}. */
@NullMarked
public class PinnedTabClosureManagerFactory {
    private static @Nullable PinnedTabClosureManager sInstanceForTesting;

    private PinnedTabClosureManagerFactory() {}

    /** Obtains the singleton instance of {@link PinnedTabClosureManager}. */
    public static PinnedTabClosureManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return PinnedTabClosureManager.getInstance();
    }

    public static void setInstanceForTesting(PinnedTabClosureManager instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null); // IN-TEST
    }
}
