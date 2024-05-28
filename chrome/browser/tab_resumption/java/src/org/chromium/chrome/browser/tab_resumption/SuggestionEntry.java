// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/** A single suggestion entry in the tab resumption module. */
public class SuggestionEntry implements Comparable<SuggestionEntry> {
    public final String sourceName;
    public final GURL url;
    public final String title;
    public final long lastActiveTime;
    public final int localTabId;

    SuggestionEntry(
            String sourceName, GURL url, String title, long lastActiveTime, int localTabId) {
        this.sourceName = sourceName;
        this.url = url;
        this.title = title;
        this.lastActiveTime = lastActiveTime;
        this.localTabId = localTabId;
    }

    /** Instantiates from `sourceName` and individual fields, assuming foreign tab. */
    @VisibleForTesting
    static SuggestionEntry createFromForeignFields(
            String sourceName, GURL url, String title, long lastActiveTime) {
        return new SuggestionEntry(
                sourceName, url, title, lastActiveTime, /* localTabId= */ Tab.INVALID_TAB_ID);
    }

    /** Instantiates from `sourceName` and ForeignSessionTab. */
    public static SuggestionEntry createFromForeignSessionTab(
            String sourceName, ForeignSessionTab foreignTab) {
        return createFromForeignFields(
                /* sourceName= */ sourceName,
                /* url= */ foreignTab.url,
                /* title= */ foreignTab.title,
                /* lastActiveTime= */ foreignTab.lastActiveTime);
    }

    /** Instantiates from Tab. */
    public static SuggestionEntry createFromLocalTab(Tab localTab) {
        return new SuggestionEntry(
                /* sourceName= */ "",
                /* url= */ localTab.getUrl(),
                /* title= */ localTab.getTitle(),
                /* lastActiveTime= */ localTab.getTimestampMillis(),
                /* localTabId= */ localTab.getId());
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
        return Integer.compare(this.localTabId, other.localTabId);
    }

    /** Returns whether the entry represents a Local Tab suggestion. */
    public boolean isLocalTab() {
        return this.localTabId != Tab.INVALID_TAB_ID;
    }
}
