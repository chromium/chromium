// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import java.util.Date;

/** Represents a recently closed entry from TabRestoreService. */
public class RecentlyClosedEntry {
    private final int mSessionId;
    private final Date mDate;

    /**
     * @param sessionId The Session ID of this entry.
     * @param timestamp The milliseconds since the Unix Epoch this entry was created.
     */
    protected RecentlyClosedEntry(int sessionId, long timestamp) {
        mSessionId = sessionId;
        mDate = new Date(timestamp);
    }

    /**
     * @return the Session ID of the entry in TabRestoreService.
     */
    public int getSessionId() {
        return mSessionId;
    }

    /**
     * @return the {@link Date} when this entry was created.
     */
    public Date getDate() {
        return mDate;
    }
}
