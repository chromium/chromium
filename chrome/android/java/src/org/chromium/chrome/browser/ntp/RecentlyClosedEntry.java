// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;

import java.util.Date;

/** Represents a recent tab or window closure event. */
@NullMarked
public class RecentlyClosedEntry {
    private final Date mDate;

    /**
     * @param timestamp The milliseconds since the Unix Epoch this entry was created.
     */
    protected RecentlyClosedEntry(long timestamp) {
        mDate = new Date(timestamp);
    }

    /**
     * @return the {@link Date} when this entry was created.
     */
    public Date getDate() {
        return mDate;
    }
}
