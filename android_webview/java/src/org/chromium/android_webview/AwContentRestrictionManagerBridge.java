// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview;

import android.net.Uri;
import android.os.ParcelFileDescriptor;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * JNI bridge for tapping into the ContentRestrictionManager system service in order to enforce
 * content restriction in WebView.
 */
@JNINamespace("android_webview")
@NullMarked
public class AwContentRestrictionManagerBridge {
    private static final String TAG = "AwCRMBridge";

    private static @Nullable Uri parseUrl(@Nullable String url) {
        if (url == null) {
            return null;
        }
        return Uri.parse(url);
    }

    @CalledByNative
    public static boolean isContentRestrictionEnabled() {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT)) {
            return false;
        }
        if (!Boolean.TRUE.equals(ManifestMetadataUtil.getContentRestrictionAppOptInPreference())) {
            return false;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            return false;
        }
        return delegate.isContentRestrictionEnabled();
    }

    @CalledByNative
    public static void requestContentClassification(
            @JniType("std::string") String url,
            @JniType("std::string") String mimeType,
            JniOnceCallback<Boolean> callback) {
        @Nullable Uri uri = parseUrl(url);
        if (uri == null) {
            callback.onResult(false);
            return;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            callback.onResult(false);
            return;
        }

        // TODO(crbug.com/481113476): Also process the request body.
        ParcelFileDescriptor requestBody = null;
        Promise<Boolean> promise =
                delegate.requestContentRestrictionClassification(
                        uri,
                        requestBody,
                        mimeType,
                        ContextUtils.getApplicationContext().getMainExecutor());
        promise.then(
                (Boolean isAllowed) -> {
                    callback.onResult(isAllowed);
                },
                (@Nullable Exception error) -> {
                    if (error != null) {
                        Log.w(TAG, "Failed to classify content", error);
                    }
                    callback.onResult(false);
                });
    }

    @CalledByNative
    public static boolean sendShowRestrictedContentIntent(@JniType("std::string") String url) {
        @Nullable Uri uri = parseUrl(url);
        if (uri == null) {
            return false;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            return false;
        }
        return delegate.sendShowRestrictedContentIntent(uri);
    }
}
