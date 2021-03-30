// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Container that holds the {@link UrlBar} and SSL state related with the current {@link Tab}.
 */
public interface LocationBar extends Destroyable {
    /** Handle all necessary tasks that can be delayed until initialization completes. */
    default void onDeferredStartup() {}

    /**
     * Call to force the UI to update the state of various buttons based on whether or not the
     * current tab is incognito.
     */
    void updateVisualsForState();

    /**
     * Sets whether the location bar should have a layout showing a title.
     *
     * @param showTitle Whether the title should be shown.
     */
    void setShowTitle(boolean showTitle);

    /**
     * Triggers the cursor to be visible in the UrlBar without triggering any of the focus animation
     * logic.
     *
     * <p>Only applies to devices with a hardware keyboard attached.
     */
    void showUrlBarCursorWithoutFocusAnimations();

    /** Selects all of the editable text in the {@link UrlBar}. */
    void selectAll();

    /**
     * Reverts any pending edits of the location bar and reset to the page state. This does not
     * change the focus state of the location bar.
     */
    void revertChanges();

    /** Returns {@link ViewGroup} that this container holds. */
    View getContainerView();

    /**
     * TODO(twellington): Try to remove this method. It's only used to return an in-product help
     *                    bubble anchor view... which should be moved out of tab and perhaps into
     *                    the status bar icon component.
     * @return The view containing the security icon.
     */
    View getSecurityIconView();


    /** Returns the {@link VoiceRecognitionHandler} associated with this LocationBar. */
    @Nullable
    default VoiceRecognitionHandler getVoiceRecognitionHandler() {
        return null;
    }
    /**
     * Returns a (@link OmniboxStub}.
     *
     * <p>TODO(crbug.com/1140287): Inject OmniboxStub where needed and remove this method.
     */
    @Nullable
    OmniboxStub getOmniboxStub();
}
