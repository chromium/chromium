// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;

/**
 * Interface implemented by feature teams to supply UI data and user action handling for the Tips
 * bottom sheet promo on Android.
 */
@NullMarked
public interface TipsPromoHandler {
    /**
     * Builds and returns the {@link FeatureTipPromoData} representing the visual promo content.
     *
     * @param context The current Android {@link Context}.
     */
    FeatureTipPromoData getPromoData(Context context);

    /** Called when the user clicks the positive action button on the promo bottom sheet. */
    void onPromoAccepted();

    /**
     * Called when the promo bottom sheet is displayed. Handlers can use the provided {@link
     * TipsPromoCustomizer} to perform optional UI adjustments, or perform non-UI lifecycle actions
     * such as recording metrics.
     *
     * @param customizer A controller allowing safe UI customizations on the promo bottom sheet.
     */
    default void onPromoShown(TipsPromoCustomizer customizer) {}

    /**
     * Functional interface used by feature teams to construct and provide their {@link
     * TipsPromoHandler}.
     */
    @FunctionalInterface
    interface Provider {
        /** Creates and returns a new {@link TipsPromoHandler}. */
        TipsPromoHandler get();
    }
}
