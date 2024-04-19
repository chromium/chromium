// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.url.GURL;

/** A single suggestion entry in the tab resumption module. */
public class SuggestionEntry implements Comparable<SuggestionEntry> {
    public final String sourceName;
    public final GURL url;
    public final String title;
    public final long lastActiveTime;
    public final int id;

    SuggestionEntry(String sourceName, GURL url, String title, long lastActiveTime, int id) {
        this.sourceName = sourceName;
        this.url = url;
        this.title = title;
        this.lastActiveTime = lastActiveTime;
        this.id = id;
    }

    /** Suggestion comparator that favors recency, and uses other fields for tie-breaking. */
    @Override
    public int compareTo(SuggestionEntry other) {
        int compareResult = Long.compare(this.lastActiveTime, other.lastActiveTime);
        if (compareResult != 0) {
            return -compareResult; // To sort by decreasing lastActiveTime.
        }
        compareResult = this.sourceName.compareTo(other.sourceName);
        if (compareResult != 0) {
            return compareResult;
        }
        compareResult = this.title.compareTo(other.title);
        if (compareResult != 0) {
            return compareResult;
        }
        return Integer.compare(this.id, other.id);
    }
}
