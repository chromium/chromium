// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.WindowDelegate;

/**
 * Provides the capabilities required to embed the omnibox suggestion list into the UI.
 */
public interface OmniboxSuggestionListEmbedder {
    /** Return the anchor view the suggestion list should be drawn below. */
    View getAnchorView();

    /**
     * Return the view that the omnibox suggestions should be aligned horizontally to.  The
     * view must be a descendant of {@link #getAnchorView()}.  If null, the suggestions will
     * be aligned to the start of {@link #getAnchorView()}.
     */
    @Nullable
    View getAlignmentView();

    /** Return the delegate used to interact with the Window. */
    WindowDelegate getWindowDelegate();

    /** Return whether the suggestions are being rendered in the tablet UI. */
    boolean isTablet();
}
