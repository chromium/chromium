// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preloading;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Java accessor for base::Features listed in
 * chrome/browser/preloading/android/preloading_feature_map.cc.
 */
@JNINamespace("chrome::preloading::android")
@NullMarked
public class PreloadingFeatureMap extends FeatureMap {
    public static final String PREWARM = "Prewarm";
    public static final String PREWARM_ZERO_SUGGEST_TRIGGER = "PrewarmZeroSuggestTrigger";
    public static final String PREWARM_PAGE_LOAD_AND_TAB_SWITCH_AFTER_PAINT_TRIGGER =
            "PrewarmPageLoadAndTabSwitchAfterPaintTrigger";

    private static final PreloadingFeatureMap sInstance = new PreloadingFeatureMap();
    private static @Nullable PreloadingFeatureMap sInstanceForTesting;

    // Do not instantiate this class.
    private PreloadingFeatureMap() {}

    /**
     * @return the singleton {@link PreloadingFeatureMap}
     */
    public static PreloadingFeatureMap getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return sInstance;
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(PreloadingFeatureMap instance) {
        sInstanceForTesting = instance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    /**
     * @return Whether prewarming should be triggered on zero-suggest prefetch.
     */
    public boolean shouldPrewarmOnZeroSuggest() {
        return isEnabled(PREWARM) && isEnabled(PREWARM_ZERO_SUGGEST_TRIGGER);
    }

    /**
     * @return Whether prewarming should be triggered on autocomplete.
     */
    public boolean shouldPrewarmOnAutocomplete() {
        return isEnabled(PREWARM)
                && !isEnabled(PREWARM_ZERO_SUGGEST_TRIGGER)
                && !isEnabled(PREWARM_PAGE_LOAD_AND_TAB_SWITCH_AFTER_PAINT_TRIGGER);
    }

    /**
     * @return Whether prewarming should be triggered on page load and tab switch after paint.
     */
    public boolean shouldPrewarmOnPageLoadAndTabSwitchAfterPaint() {
        return isEnabled(PREWARM)
                && !isEnabled(PREWARM_ZERO_SUGGEST_TRIGGER)
                && isEnabled(PREWARM_PAGE_LOAD_AND_TAB_SWITCH_AFTER_PAINT_TRIGGER);
    }

    @Override
    protected long getNativeMap() {
        return PreloadingFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
