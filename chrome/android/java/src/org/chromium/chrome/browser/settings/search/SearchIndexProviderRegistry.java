// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * A central registry that holds the list of all SearchIndexProvider instances. This is the single
 * source of truth for the search indexing process.
 */
@NullMarked
public final class SearchIndexProviderRegistry {
    /**
     * The list of all providers that can be indexed for settings search.
     *
     * <p>When you create a new searchable PreferenceFragment, you must add its
     * SEARCH_INDEX_DATA_PROVIDER to this list.
     */
    public static final List<SearchIndexProvider> ALL_PROVIDERS =
            List.of(org.chromium.chrome.browser.settings.MainSettings.SEARCH_INDEX_DATA_PROVIDER);
}
