// Copyright 2022 The Chromium Authors. All rights reserved.
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

    private final List<String> mKeywords;
    private final List<ClusterVisit> mVisits;
    private final String mLabel;
    private final List<MatchPosition> mMatchPositions;
    private final long mTimestamp;
    private final List<String> mRelatedSearches;

    public HistoryCluster(List<String> keywords, List<ClusterVisit> visits, String label,
            List<MatchPosition> matchPositions, long timestamp, List<String> relatedSearches) {
        mKeywords = keywords;
        mVisits = visits;
        mLabel = label;
        mMatchPositions = matchPositions;
        mTimestamp = timestamp;
        mRelatedSearches = relatedSearches;
    }

    public List<ClusterVisit> getVisits() {
        return mVisits;
    }

    public String getLabel() {
        return mLabel;
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
