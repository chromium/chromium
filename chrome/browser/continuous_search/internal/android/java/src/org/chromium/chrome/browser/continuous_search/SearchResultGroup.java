// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import java.util.List;

/**
 * A class that holds a group of related {@link SearchResult}s.
 */
public class SearchResultGroup {
    private final String mLabel;
    private final boolean mIsAdGroup;
    private final List<SearchResult> mResults;

    SearchResultGroup(String label, boolean isAdGroup, List<SearchResult> results) {
        mLabel = label;
        mIsAdGroup = isAdGroup;
        mResults = results;
    }

    String getLabel() {
        return mLabel;
    }

    boolean isAdGroup() {
        return mIsAdGroup;
    }

    List<SearchResult> getResults() {
        return mResults;
    }
}
