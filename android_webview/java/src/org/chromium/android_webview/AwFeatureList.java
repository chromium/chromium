// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("android_webview")
@MainDex
final public class AwFeatureList {
    // Do not instantiate this class.
    private AwFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in android_webview/browser/aw_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return nativeIsEnabled(featureName);
    }

    // Alphabetical:
    public static final String WEBVIEW_CONNECTIONLESS_SAFE_BROWSING =
            "WebViewConnectionlessSafeBrowsing";

    private static native boolean nativeIsEnabled(String featureName);
}
