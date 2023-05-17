// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for base/feature_map.h state.
 */
@JNINamespace("android_webview")
public final class AwFeatureMap extends FeatureMap {
    private static final AwFeatureMap sInstance = new AwFeatureMap();

    // Do not instantiate this class.
    private AwFeatureMap() {}

    /**
     * @return the singleton UiAndroidFeatureMap.
     */
    public static AwFeatureMap getInstance() {
        return sInstance;
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
