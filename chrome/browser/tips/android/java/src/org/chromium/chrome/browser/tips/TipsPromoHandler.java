// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.Collections;
import java.util.Map;

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

    /**
     * Called when the user clicks the positive action button on the promo bottom sheet.
     *
     * @param context The current Android {@link Context}, typically the hosting activity, which can
     *     be used to launch the feature's UI (e.g. a settings page).
     */
    void onPromoAccepted(Context context);

    /**
     * Called when the promo bottom sheet is displayed. Handlers can use the provided {@link
     * TipsPromoCustomizer} to perform optional UI adjustments, or perform non-UI lifecycle actions
     * such as recording metrics.
     *
     * @param customizer A controller allowing safe UI customizations on the promo bottom sheet.
     */
    default void onPromoShown(TipsPromoCustomizer customizer) {}

    /**
     * Returns runtime signals that can only be retrieved in Java (e.g. current UI state) and that
     * this feature's native {@code TipsFeature} consumes when determining eligibility. Signals
     * retrievable natively (prefs, UMA, etc.) should instead be declared on the native side.
     *
     * <p>Keys must match the signal names expected by the native {@code TipsFeature::IsEligible}
     * implementation. The returned map is merged with those of all other registered handlers and
     * passed across JNI as a single map, so keys should be unique per feature.
     *
     * <p>TODO(crbug.com/559732659): Define signal keys once in a shared native header (e.g.
     * //chrome/browser/tips/core/tips_signal_keys.h) and generate the Java constants via
     * java_cpp_strings, rather than borrowing SegmentationPlatformConstants.
     *
     * <p>This is invoked at notification scheduling time (on startup and periodically while Chrome
     * is in the foreground), not when the promo is shown, so implementations must be cheap and free
     * of side effects.
     *
     * @param windowAndroid The current {@link WindowAndroid}, which can be used to look up
     *     window-scoped state.
     * @return A map of signal name to value, or an empty map if the feature has no Java-only
     *     signals.
     */
    default Map<String, Float> getCustomSignals(WindowAndroid windowAndroid) {
        return Collections.emptyMap();
    }

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
