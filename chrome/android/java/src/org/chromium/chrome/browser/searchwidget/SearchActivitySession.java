// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;

/**
 * Models a single search served by {@link SearchActivity}.
 *
 * <p>SearchActivity is single-task, so one activity instance serves many searches: every new
 * intent is delivered through {@code onNewIntent}. A session holds the immutable facts about the
 * intent that started one search, and is replaced whenever a new intent arrives.
 */
@NullMarked
public class SearchActivitySession {
    private final @IntentOrigin int mIntentOrigin;
    private final @SearchType int mSearchType;

    /**
     * @param intent The intent that started this session.
     */
    public SearchActivitySession(Intent intent) {
        mIntentOrigin = SearchActivityUtils.getIntentOrigin(intent);
        mSearchType = SearchActivityUtils.getIntentSearchType(intent);
    }

    /** Returns the component that launched this session. */
    public @IntentOrigin int getIntentOrigin() {
        return mIntentOrigin;
    }

    /** Returns the search type requested for this session. */
    public @SearchType int getSearchType() {
        return mSearchType;
    }
}
