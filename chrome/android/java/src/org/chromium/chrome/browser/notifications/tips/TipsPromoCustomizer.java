// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import androidx.annotation.DimenRes;
import androidx.annotation.RawRes;

import org.chromium.build.annotations.NullMarked;

/**
 * Controller interface provided to {@link TipsPromoHandler#onPromoShown} allowing feature handlers
 * to perform UI customizations on the Tips promo bottom sheet without direct access to internal
 * view hierarchies.
 */
@NullMarked
public interface TipsPromoCustomizer {
    /**
     * Plays a looping Lottie animation in the promo's logo view.
     *
     * @param animationRes The raw resource ID of the Lottie animation.
     */
    void setLogoAnimation(@RawRes int animationRes);

    /**
     * Sets custom top padding on the promo's logo view.
     *
     * @param paddingDimenRes The dimension resource ID for the top padding.
     */
    void setLogoTopPadding(@DimenRes int paddingDimenRes);

    /**
     * Sets the visibility of the details button on the main screen.
     *
     * @param visible Whether the details button should be visible.
     */
    void setDetailsButtonVisibility(boolean visible);

    /**
     * Sets the visibility of the description text on the main screen.
     *
     * @param visible Whether the description text should be visible.
     */
    void setDescriptionVisibility(boolean visible);
}
