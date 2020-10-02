// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

/**
 * Handler for actions that happen on suggestion view.
 */
public interface SuggestionViewDelegate {
    /** Triggered when the user selects one of the omnibox suggestions to navigate to. */
    void onSelection();

    /** Triggered when the user long presses the omnibox suggestion. */
    void onLongPress();
}
