// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;

/**
 * JNI bridge for tapping into the ContentRestrictionManager system service in order to enforce
 * content restriction in WebView.
 */
@JNINamespace("android_webview")
@NullMarked
public class AwContentRestrictionManagerBridge {
    private static final String TAG = "AwCRMBridge";

    @CalledByNative
    public static boolean isContentRestrictionEnabled() {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT)) {
            Log.w(TAG, "Content restriction feature support is disabled.");
            return false;
        }
        AconfigFlaggedApiDelegate delegate =
                ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            return false;
        }
        return delegate.isContentRestrictionEnabled();
    }
}
