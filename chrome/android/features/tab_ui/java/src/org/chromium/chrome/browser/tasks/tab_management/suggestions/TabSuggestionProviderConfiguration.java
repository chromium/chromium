// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

/**
 * Encapsulates signals indicating how useful the suggestions from a suggestions
 * provider are. Used for aggregating and determining which suggestions to use
 */
class TabSuggestionProviderConfiguration {
    public final int score;
    public final boolean isEnabled;

    TabSuggestionProviderConfiguration(int score, boolean isEnabled) {
        this.score = score;
        this.isEnabled = isEnabled;
    }
}
