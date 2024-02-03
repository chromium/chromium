// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base/android/feature_map.h state. */
@JNINamespace("android_webview")
public final class AwFeatureMap extends FeatureMap {
    private static final AwFeatureMap sInstance = new AwFeatureMap();

    // Do not instantiate this class.
    private AwFeatureMap() {}

    /** @return the singleton UiAndroidFeatureMap. */
    public static AwFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return AwFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
