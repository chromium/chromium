// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import java.util.List;

class HistoryCluster {
    private final List<String> mKeywords;
    private final List<ClusterVisit> mVisits;

    public HistoryCluster(List<String> keywords, List<ClusterVisit> visits) {
        mKeywords = keywords;
        mVisits = visits;
    }

    public List<ClusterVisit> getVisits() {
        return mVisits;
    }
}
