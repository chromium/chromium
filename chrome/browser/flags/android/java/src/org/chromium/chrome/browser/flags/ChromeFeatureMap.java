// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/**
 * Java accessor for state of Chrome-layer feature flags.
 *
 * This class provides methods to access values of Chrome-layer feature flags, listed in
 * {@link ChromeFeatureList} and to  their field trial parameters. The API to access those values
 * is in the base class {@link FeatureMap} and is shared with other FeatureLists and FeatureMaps.
 *
 * The same functionality is provided through static methods in {@link ChromeFeatureList} for
 * backwards compatibility and convenience.
 */
@JNINamespace("chrome::android")
public class ChromeFeatureMap extends FeatureMap {
    private static final ChromeFeatureMap sInstance = new ChromeFeatureMap();

    /** Prevent instantiation. */
    private ChromeFeatureMap() {
        super();
    }

    public static ChromeFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return ChromeFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    interface Natives {
        long getNativeMap();
    }
}
