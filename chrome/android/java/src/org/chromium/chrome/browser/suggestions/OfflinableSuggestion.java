// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import androidx.annotation.Nullable;

/**
 * Exposes the data of a suggestion that can be saved offline.
 */
public interface OfflinableSuggestion {
    /** @return The URL of this suggestion. */
    String getUrl();

    /** Assigns an offline page id to the suggestion. Set to {@code null} to clear. */
    void setOfflinePageOfflineId(@Nullable Long offlineId);

    /** @return current offline id assigned to the suggestion, or {@code null} if there is none. */
    @Nullable
    Long getOfflinePageOfflineId();

    /**
     * @return whether a suggestion has to be matched with the exact offline page or with the most
     * recent offline page found by the suggestion's URL.
     */
    boolean requiresExactOfflinePage();
}
