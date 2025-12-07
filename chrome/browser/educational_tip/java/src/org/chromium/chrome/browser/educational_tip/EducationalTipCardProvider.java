// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;

/** The interface for a card which is shown in the educational tip module. */
@NullMarked
public interface EducationalTipCardProvider {
    /** Gets the title of the card. */
    String getCardTitle();

    /** Gets the description of the card. */
    String getCardDescription();

    /** Gets the string for the button of the card. */
    String getCardButtonText();

    /** Gets the image of the card. */
    @DrawableRes
    int getCardImage();

    /** Called when the user clicks a module button. */
    void onCardClicked();

    /** Called when the module is hidden. */
    default void destroy() {}
}
