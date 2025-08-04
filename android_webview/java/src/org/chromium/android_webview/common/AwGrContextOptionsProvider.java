// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import static org.chromium.base.metrics.RecordHistogram.recordBooleanHistogram;

import android.content.pm.PackageManager;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;

/**
 * Java counterpart to the native class of the same name. This class provides utility and shouldn't
 * hold any state or be instantiated.
 */
class AwGrContextOptionsProvider {
    private static final String HISTOGRAM_NAME = "Android.WebView.EnableTVSmoothing";

    /** Returns true if the device supports leanback, the feature used by TV devices. */
    @CalledByNative
    private static boolean shouldEnableTvSmoothing() {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        boolean isTv = pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK);
        // Record whether we enabled smoothing so that we can check which devices are seeing
        // smoothing and refine isTv over time.
        recordBooleanHistogram(HISTOGRAM_NAME, isTv);
        // Check isTV first so that only TVs will check the feature flag.
        return isTv && AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_USE_RENDERING_HEURISTIC);
    }

    private AwGrContextOptionsProvider() {}
}
