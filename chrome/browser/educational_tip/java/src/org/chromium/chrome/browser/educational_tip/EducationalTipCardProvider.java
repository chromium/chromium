// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.hub.PaneId;

/** The interface for a card which is shown in the educational tip module. */
public interface EducationalTipCardProvider {
    // A callback to open the provided pane Id of the Hub layout.
    // TODO(https://crbug.com/382803396): Check if this is related to the crash that occurs when
    // clicking the tab group sync card; if not, delete it.
    @FunctionalInterface
    interface ShowHubPaneCallback {
        void onClick(@PaneId int paneId);
    }

    /** Gets the title of the card. */
    String getCardTitle();

    /** Gets the description of the card. */
    String getCardDescription();

    /** Gets the image of the card. */
    @DrawableRes
    int getCardImage();

    /** Called when the user clicks a module button. */
    void onCardClicked();

    /** Called when the module is hidden. */
    default void destroy() {}
}
