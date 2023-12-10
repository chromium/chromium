// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import java.util.List;

class HistoryCluster {
    static class MatchPosition {
        final int mMatchStart;
        final int mMatchEnd;

        public MatchPosition(int matchStart, int matchEnd) {
            mMatchStart = matchStart;
            mMatchEnd = matchEnd;
        }
    }

    private final List<ClusterVisit> mVisits;
    private final String mLabel;
    private final List<MatchPosition> mMatchPositions;
    private final long mTimestamp;
    private final List<String> mRelatedSearches;
    private final String mRawLabel;

    public HistoryCluster(
            List<ClusterVisit> visits,
            String label,
            String rawLabel,
            List<MatchPosition> matchPositions,
            long timestamp,
            List<String> relatedSearches) {
        mVisits = visits;
        mLabel = label;
        mRawLabel = rawLabel;
        mMatchPositions = matchPositions;
        mTimestamp = timestamp;
        mRelatedSearches = relatedSearches;

        for (int i = 0; i < mVisits.size(); i++) {
            mVisits.get(i).setIndexInParent(i);
        }
    }

    public List<ClusterVisit> getVisits() {
        return mVisits;
    }

    public String getLabel() {
        return mLabel;
    }

    public String getRawLabel() {
        return mRawLabel;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public List<String> getRelatedSearches() {
        return mRelatedSearches;
    }

    public List<MatchPosition> getMatchPositions() {
        return mMatchPositions;
    }
}
