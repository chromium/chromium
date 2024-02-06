// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.url.GURL;

/** A single suggestion entry in the tab resumption module. */
public class SuggestionEntry implements Comparable<SuggestionEntry> {
    public final String sourceName;
    public final GURL url;
    public final String title;
    public final long timestamp;
    public final int id;

    // Cached Drawable to reduce churn from image fetching and processing with RecyclerView use.
    @Nullable private Drawable mUrlDrawable;

    SuggestionEntry(String sourceName, GURL url, String title, long timestamp, int id) {
        this.sourceName = sourceName;
        this.url = url;
        this.title = title;
        this.timestamp = timestamp;
        this.id = id;
    }

    /** Suggestion comparator that favors recency, and uses other fields for tie-breaking. */
    @Override
    public int compareTo(SuggestionEntry other) {
        int compareResult = Long.compare(this.timestamp, other.timestamp);
        if (compareResult != 0) {
            return -compareResult; // To sort by decreasing timestamp.
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

    /**
     * @return Cached URL Drawable.
     */
    public Drawable getUrlDrawable() {
        return mUrlDrawable;
    }

    /** Sets the cached URL Drawable. */
    public void setUrlDrawable(Drawable urlDrawable) {
        this.mUrlDrawable = urlDrawable;
    }
}
