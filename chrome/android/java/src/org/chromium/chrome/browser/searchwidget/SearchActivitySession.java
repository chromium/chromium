// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.components.omnibox.AutocompleteRequestType;

/**
 * Models a single search served by {@link SearchActivity}.
 *
 * <p>SearchActivity is single-task, so one activity instance serves many searches: every new intent
 * is delivered through {@code onNewIntent}. A session holds the immutable facts about the intent
 * that started one search, and is replaced whenever a new intent arrives. Sessions are built from
 * intents by {@link SearchActivityUtils#getIntentSession}.
 */
@NullMarked
public class SearchActivitySession {
    /** The component that launched this session. */
    public final @IntentOrigin int intentOrigin;

    /** The search type requested for this session. */
    public final @SearchType int searchType;

    /** The request type the Omnibox input should begin in for this session. */
    public final @AutocompleteRequestType int requestType;

    /** Initial query text, or null when the Omnibox should start empty. */
    public final @Nullable String query;

    public SearchActivitySession(
            @IntentOrigin int intentOrigin,
            @SearchType int searchType,
            @AutocompleteRequestType int requestType,
            @Nullable String query) {
        this.intentOrigin = intentOrigin;
        this.searchType = searchType;
        this.requestType = requestType;
        this.query = query;
    }

    /**
     * Creates a session for a search initiated from the Tab Switcher Hub.
     *
     * @param query Initial query text, or null to start with an empty Omnibox.
     */
    public static SearchActivitySession forHub(@Nullable String query) {
        return new SearchActivitySession(
                IntentOrigin.HUB, SearchType.TEXT, AutocompleteRequestType.SEARCH, query);
    }
}
