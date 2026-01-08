// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.build.annotations.NullMarked;

/** Factory for creating {@link RecentlyClosedEntriesManagerTracker}. */
@NullMarked
public final class RecentlyClosedEntriesManagerTrackerFactory {
    private RecentlyClosedEntriesManagerTrackerFactory() {}

    /** Obtains the singleton instance of {@link RecentlyClosedEntriesManagerTracker}. */
    public static RecentlyClosedEntriesManagerTracker getInstance() {
        return RecentlyClosedEntriesManagerTrackerImpl.getInstance();
    }
}
