// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

import java.util.Locale;

/**
 * An {@link OfflineItemFilter} responsible for pruning out items that don't match a specific search
 * query.
 */
@NullMarked
public class SearchOfflineItemFilter extends OfflineItemFilter {
    private @Nullable String mQuery;

    /** Creates an instance of this fitler and wraps {@code source}. */
    public SearchOfflineItemFilter(OfflineItemFilterSource source) {
        super(source);
        onFilterChanged();
    }

    /**
     * Sets the filter query to {@code query}.  Note that if {@code query} is empty or {@code null}
     * this filter will allow all {@link OfflineItem}s through.
     * @param query The new query string to filter on.
     */
    public void onQueryChanged(@Nullable String query) {
        if (query == null) query = "";

        query = query.toLowerCase(Locale.getDefault());
        if (TextUtils.equals(mQuery, query)) return;

        mQuery = query;
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        if (TextUtils.isEmpty(mQuery)) return false;
        String url = item.originalUrl == null ? null : item.originalUrl.getSpec();
        return !fieldContainsQuery(formatUrl(url)) && !fieldContainsQuery(item.title);
    }

    private boolean fieldContainsQuery(@Nullable String field) {
        if (TextUtils.isEmpty(field)) return false;

        return field.toLowerCase(Locale.getDefault()).contains(mQuery);
    }

    /** Visible to allow tests to avoid calls to native. */
    @VisibleForTesting
    protected String formatUrl(@Nullable String url) {
        return UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
    }
}
