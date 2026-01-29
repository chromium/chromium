// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preloading;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

/**
 * Java accessor for base::Features listed in
 * chrome/browser/preloading/android/preloading_feature_map.cc.
 */
@JNINamespace("chrome::preloading::android")
@NullMarked
public class PreloadingFeatureMap extends FeatureMap {
    private static final PreloadingFeatureMap sInstance = new PreloadingFeatureMap();

    // Do not instantiate this class.
    private PreloadingFeatureMap() {}

    /**
     * @return the singleton {@link PreloadingFeatureMap}
     */
    public static PreloadingFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    /**
     * @return Whether prewarming should be triggered on zero-suggest prefetch.
     */
    public boolean shouldPrewarmOnZeroSuggest() {
        return isEnabled("Prewarm")
                && getFieldTrialParamByFeatureAsBoolean("Prewarm", "zero_suggest_trigger", false);
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
