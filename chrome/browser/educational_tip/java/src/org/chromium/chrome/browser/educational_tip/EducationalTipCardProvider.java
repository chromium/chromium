// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;

import org.chromium.chrome.browser.hub.PaneId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The interface for a card which is shown in the educational tip module. */
public interface EducationalTipCardProvider {
    // A callback to open the provided pane Id of the Hub layout.
    @FunctionalInterface
    interface ShowHubPaneCallback {
        void onClick(@PaneId int paneId);
    }

    /** Card types that are shown in the educational tip module on the magic stack. */
    @IntDef({
        EducationalTipCardType.DEFAULT_BROWSER_PROMO,
        EducationalTipCardType.TAB_GROUPS,
        EducationalTipCardType.TAB_GROUP_SYNC,
        EducationalTipCardType.QUICK_DELETE,
        EducationalTipCardType.NUM_ENTRIES,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface EducationalTipCardType {
        int DEFAULT_BROWSER_PROMO = 0;
        int TAB_GROUPS = 1;
        int TAB_GROUP_SYNC = 2;
        int QUICK_DELETE = 3;
        int NUM_ENTRIES = 4;
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
