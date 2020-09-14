// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.chromium.chrome.browser.WindowDelegate;

/** Represents the basic functionality of the Omnibox suggestions dropdown. */
public interface OmniboxSuggestionsDropdown {
    /** Provides the capabilities required to embed the omnibox suggestion list into the UI. */
    public interface Embedder {
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

    /** Interface that will receive notifications and callbacks from OmniboxSuggestionList. */
    public interface Observer {
        /**
         * Invoked whenever the height of suggestion list changes.
         * The height may change as a result of eg. soft keyboard popping up.
         *
         * @param newHeightPx New height of the suggestion list in pixels.
         */
        void onSuggestionDropdownHeightChanged(@Px int newHeightPx);

        /**
         * Invoked whenever the User scrolls the list.
         */
        void onSuggestionDropdownScroll();

        /**
         * Invoked whenever the User scrolls the list to the top.
         */
        void onSuggestionDropdownOverscrolledToTop();
    }

    /** Get the Android View implementing suggestion list. */
    ViewGroup getViewGroup();

    /** Show (and properly size) the suggestions list. */
    void show();

    /** Hide the suggestions list and release any cached resources. */
    void hide();

    /**
     * Sets the embedder for the list view.
     * @param embedder the embedder of this list.
     */
    void setEmbedder(Embedder embedder);

    /**
     * Sets the observer of suggestion list.
     * @param observer an observer of this list.
     */
    void setObserver(Observer observer);

    /** Resets selection typically in response to changes to the list. */
    void resetSelection();

    /** Update the suggestion popup background to reflect the current state. */
    void refreshPopupBackground(boolean isIncognito);

    /** Gets the number of items in the list. */
    int getItemCount();
}
