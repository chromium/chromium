// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.pm.PackageManager;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;

/**
 * Java counterpart to the native class of the same name. This class provides utility and shouldn't
 * hold any state or be instantiated.
 */
class AwGrContextOptionsProvider {

    /** Returns true if the device supports leanback, the feature used by TV devices. */
    @CalledByNative
    private static boolean shouldEnableTvSmoothing() {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK);
    }

    private AwGrContextOptionsProvider() {}
}
