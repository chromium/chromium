// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;

/** Represents a recent tab or window closure event. */
@NullMarked
public class RecentlyClosedEntry {
    private final long mTimestamp;

    /**
     * @param timestamp The milliseconds since the Unix Epoch this entry was created.
     */
    protected RecentlyClosedEntry(long timestamp) {
        mTimestamp = timestamp;
    }

    /**
     * @return the timestamp in milliseconds since the Unix Epoch when this entry was created.
     */
    public long getTimestamp() {
        return mTimestamp;
    }
}
