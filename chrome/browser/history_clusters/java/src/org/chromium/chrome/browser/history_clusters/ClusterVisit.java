// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.url.GURL;

import java.util.List;

class ClusterVisit {
    private final float mScore;
    private final String mTitle;
    private final String mUrlForDisplay;
    private final GURL mNormalizedUrl;
    private final List<MatchPosition> mTitleMatchPositions;
    private final List<MatchPosition> mUrlMatchPositions;

    public ClusterVisit(float score, GURL normalizedUrl, String title, String urlForDisplay,
            List<MatchPosition> titleMatchPositions, List<MatchPosition> urlMatchPositions) {
        mScore = score;
        mNormalizedUrl = normalizedUrl;
        mTitle = title;
        mUrlForDisplay = urlForDisplay;
        mTitleMatchPositions = titleMatchPositions;
        mUrlMatchPositions = urlMatchPositions;
    }

    public String getTitle() {
        return mTitle;
    }

    public GURL getGURL() {
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
}
