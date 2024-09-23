// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

@JNINamespace("base::android")
public class BaseFeatureMap extends FeatureMap {
    private static final BaseFeatureMap sInstance = new BaseFeatureMap();

    // Do not instantiate this class.
    private BaseFeatureMap() {}

    /**
     * @return the singleton UiAndroidFeatureMap.
     */
    public static BaseFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
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
