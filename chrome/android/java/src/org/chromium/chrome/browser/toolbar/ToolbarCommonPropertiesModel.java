// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ntp.NewTabPage;

/**
 * Defines an interface that provides common properties to toolbar and omnibox classes.
 */
public interface ToolbarCommonPropertiesModel {
    /**
     * @return The current url for the current tab. Returns empty string when there is no tab.
     */
    @NonNull
    String getCurrentUrl();

    /**
     * @return The NewTabPage shown for the current Tab or null if one is not being shown.
     */
    NewTabPage getNewTabPageForCurrentTab();

    /**
     * @return Whether the toolbar is currently being displayed for incognito.
     */
    boolean isIncognito();

    /** @return Whether the current {@link Tab} is loading. */
    boolean isLoading();

    /**
     * If the current tab state is eligible for displaying the search query terms instead of the
     * URL, this extracts the query terms from the current URL.
     *
     * @return The search terms. Returns null if the tab is ineligible to display the search terms
     *         instead of the URL.
     */
    @Nullable
    default String getDisplaySearchTerms() {
        return null;
    }
}
