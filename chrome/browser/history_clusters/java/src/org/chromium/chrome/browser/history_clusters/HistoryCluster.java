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

    public HistoryCluster(List<String> keywords, List<ClusterVisit> visits, String label,
            List<MatchPosition> matchPositions) {
        mKeywords = keywords;
        mVisits = visits;
        mLabel = label;
        mMatchPositions = matchPositions;
    }

    public List<ClusterVisit> getVisits() {
        return mVisits;
    }

    public String getLabel() {
        return mLabel;
    }
}
