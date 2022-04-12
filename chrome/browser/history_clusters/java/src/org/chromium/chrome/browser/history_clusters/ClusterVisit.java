// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import org.chromium.url.GURL;

class ClusterVisit {
    private final float mScore;
    private final String mTitle;
    private final GURL mNormalizedUrl;

    public ClusterVisit(float score, GURL normalizedUrl, String title) {
        mScore = score;
        mNormalizedUrl = normalizedUrl;
        mTitle = title;
    }

    public String getTitle() {
        return mTitle;
    }

    public GURL getGURL() {
        return mNormalizedUrl;
    }
}
