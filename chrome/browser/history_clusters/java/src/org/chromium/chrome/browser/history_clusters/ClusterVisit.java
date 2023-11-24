// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.url.GURL;

import java.util.List;

/** Data structure representing a single logical visit and associated annotations//display data. */
public class ClusterVisit {
    /**
     * Data structure representing a visit that is considered a duplicate of a more relevant visit.
     */
    public static class DuplicateVisit {
        private final long mTimestamp;
        private final GURL mUrl;

        DuplicateVisit(long timestamp, GURL url) {
            mTimestamp = timestamp;
            mUrl = url;
        }

        public long getTimestamp() {
            return mTimestamp;
        }

        public GURL getUrl() {
            return mUrl;
        }
    }

    private final float mScore;
    private final String mTitle;
    private final String mUrlForDisplay;
    private final GURL mNormalizedUrl;
    private final List<MatchPosition> mTitleMatchPositions;
    private final List<MatchPosition> mUrlMatchPositions;
    private final GURL mRawUrl;
    private final long mTimestamp;
    private final List<DuplicateVisit> mDuplicateVisits;
    private int mIndexInParent;

    /**
     * Create a new ClusterVisit.
     * @param score A floating point score in the range [0, 1] describing how important this visit
     *         is to the containing cluster.
     * @param normalizedUrl The normalized URL for the visit, suitable for using to return to the
     *         page.
     * @param title The title of the page visited.
     * @param urlForDisplay Url to display to the user.
     * @param titleMatchPositions Which positions matched the search query in the title.
     * @param urlMatchPositions Which positions matched the search query in the url to display.
     * @param rawUrl The un-normalized (i.e. exact) URL of the visit.
     * @param timestamp The time at which the most visit occurred.
     * @param duplicateVisits A list of visits that have been de-duplicated into this visit.
     */
    public ClusterVisit(
            float score,
            GURL normalizedUrl,
            String title,
            String urlForDisplay,
            List<MatchPosition> titleMatchPositions,
            List<MatchPosition> urlMatchPositions,
            GURL rawUrl,
            long timestamp,
            List<DuplicateVisit> duplicateVisits) {
        mScore = score;
        mNormalizedUrl = normalizedUrl;
        mTitle = title;
        mUrlForDisplay = urlForDisplay;
        mTitleMatchPositions = titleMatchPositions;
        mUrlMatchPositions = urlMatchPositions;
        mRawUrl = rawUrl;
        mTimestamp = timestamp;
        mDuplicateVisits = duplicateVisits;
    }

    public String getTitle() {
        return mTitle;
    }

    public GURL getNormalizedUrl() {
        return mNormalizedUrl;
    }

    public String getUrlForDisplay() {
        return mUrlForDisplay;
    }

    public List<MatchPosition> getTitleMatchPositions() {
        return mTitleMatchPositions;
    }

    public List<MatchPosition> getUrlMatchPositions() {
        return mUrlMatchPositions;
    }

    public GURL getRawUrl() {
        return mRawUrl;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public List<DuplicateVisit> getDuplicateVisits() {
        return mDuplicateVisits;
    }

    public void setIndexInParent(int index) {
        mIndexInParent = index;
    }

    public int getIndexInParent() {
        return mIndexInParent;
    }
}
