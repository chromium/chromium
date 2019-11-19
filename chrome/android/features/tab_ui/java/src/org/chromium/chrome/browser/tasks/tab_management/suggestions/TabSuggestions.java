// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

/**
 * Interface for the Tab Suggestions framework.
 */
public interface TabSuggestions {
    /**
     * Adds an observer
     * @param tabSuggestionsObserver observer which is notified when new suggestions are available
     */
    void addObserver(TabSuggestionsObserver tabSuggestionsObserver);

    /**
     * Removes an observer
     * @param tabSuggestionsObserver observer which is notified when new suggestions are available
     */
    void removeObserver(TabSuggestionsObserver tabSuggestionsObserver);
}
