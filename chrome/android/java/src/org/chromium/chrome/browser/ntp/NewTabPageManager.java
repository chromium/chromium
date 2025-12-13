// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.components.omnibox.AutocompleteRequestType;

/** Manages the view interaction with the rest of the system. */
@NullMarked
public interface NewTabPageManager extends SuggestionsUiDelegate {
    /** Returns whether the location bar is shown in the NTP. */
    boolean isLocationBarShownInNtp();

    /** Returns whether voice search is enabled and the microphone should be shown. */
    boolean isVoiceSearchEnabled();

    /**
     * Animates the search box up into the omnibox and bring up the keyboard.
     *
     * @param beginVoiceSearch Whether to begin a voice search.
     * @param requestType Type of request the focused omnibox should begin serving.
     * @param pastedText Text to paste in the omnibox after it's been focused. May be null.
     */
    void focusSearchBox(
            boolean beginVoiceSearch,
            @AutocompleteRequestType int requestType,
            @Nullable String pastedText);

    /**
     * Returns whether the {@link NewTabPage} associated with this manager is the current page
     * displayed to the user.
     */
    boolean isCurrentPage();

    /**
     * Called when the NTP has completely finished loading (all views will be inflated
     * and any dependent resources will have been loaded).
     */
    void onLoadingComplete();
}
