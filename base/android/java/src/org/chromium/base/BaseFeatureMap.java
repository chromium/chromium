// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base::Features listed in {@link BaseFeatures}
 */
@JNINamespace("base::android")
public final class BaseFeatureMap extends FeatureMap {
    private static final BaseFeatureMap sInstance = new BaseFeatureMap();

    // Do not instantiate this class.
    private BaseFeatureMap() {}

    /**
     * @return the singleton DeviceFeatureMap.
     */
    public static BaseFeatureMap getInstance() {
        return sInstance;
    }

    /**
     * Convenience method to call {@link #isEnabledInNative(String)} statically.
     */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return BaseFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
