// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview;

import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Supplier;

/**
 * JNI bridge for tapping into the ContentRestrictionManager system service in order to enforce
 * content restriction in WebView. This class is owned and instantiated by the
 * AwContentRestrictionManagerClient (C++).
 */
@Lifetime.Profile
@JNINamespace("android_webview")
@NullMarked
public class AwContentRestrictionManagerBridge {
    private static final String TAG = "AwCRMBridge";

    @VisibleForTesting
    public interface ParcelFileDescriptorPipeFactory {
        ParcelFileDescriptor[] createPipe() throws IOException;
    }

    private static ParcelFileDescriptorPipeFactory sPipeFactory =
            new ParcelFileDescriptorPipeFactory() {
                @Override
                public ParcelFileDescriptor[] createPipe() throws IOException {
                    return ParcelFileDescriptor.createPipe();
                }
            };

    private static @Nullable Supplier<@Nullable AconfigFlaggedApiDelegate>
            sDelegateSupplierForTesting;

    private final Map<Long, ParcelFileDescriptor> mReadFileDescriptorMap = new HashMap<>();

    @CalledByNative
    public AwContentRestrictionManagerBridge() {}

    private static @Nullable Uri parseUrl(@Nullable String url) {
        if (url == null) {
            return null;
        }
        return Uri.parse(url);
    }

    private static @Nullable AconfigFlaggedApiDelegate getDelegate() {
        if (sDelegateSupplierForTesting != null) {
            return sDelegateSupplierForTesting.get();
        }
        return AconfigFlaggedApiDelegate.getInstance();
    }

    @CalledByNative
    public void destroy() {
        for (ParcelFileDescriptor pfd : mReadFileDescriptorMap.values()) {
            try {
                pfd.close();
            } catch (IOException e) {
                Log.e(TAG, e);
            }
        }
        mReadFileDescriptorMap.clear();
    }

    @CalledByNative
    public int createRequestBodyPipeAndGetWriteFd(long navigationId) {
        try {
            ParcelFileDescriptor[] fdPipes = sPipeFactory.createPipe();
            mReadFileDescriptorMap.put(navigationId, fdPipes[0]);
            return fdPipes[1].detachFd();
        } catch (IOException e) {
            Log.e(TAG, "Failed to create pipe", e);
            return -1;
        }
    }

    @CalledByNative
    public boolean isContentRestrictionEnabled() {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT)) {
            return false;
        }
        boolean appOptIn =
                Boolean.TRUE.equals(ManifestMetadataUtil.getContentRestrictionAppOptInPreference());
        RecordHistogram.recordBooleanHistogram(
                "Android.WebView.ContentRestriction.AppOptIn", appOptIn);
        if (!appOptIn) {
            return false;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = getDelegate();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            return false;
        }
        return delegate.isContentRestrictionEnabled();
    }

    @CalledByNative
    public void requestContentClassification(
            long navigationId,
            @JniType("std::string") String url,
            @JniType("std::string") String mimeType,
            JniOnceCallback<Boolean> callback) {
        @Nullable Uri uri = parseUrl(url);
        if (uri == null) {
            callback.onResult(true);
            return;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = getDelegate();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            callback.onResult(true);
            return;
        }

        ParcelFileDescriptor requestBody = mReadFileDescriptorMap.remove(navigationId);
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
                    callback.onResult(true);
                });

        // Close the request body file descriptor soon after it has been sent to the platform.
        if (requestBody != null) {
            try {
                requestBody.close();
            } catch (IOException e) {
                Log.e(TAG, e);
            }
        }
    }

    @CalledByNative
    public boolean sendShowRestrictedContentIntent(@JniType("std::string") String url) {
        @Nullable Uri uri = parseUrl(url);
        if (uri == null) {
            return false;
        }
        @Nullable AconfigFlaggedApiDelegate delegate = getDelegate();
        if (delegate == null) {
            Log.w(TAG, "Unable to retrieve the AconfigFlaggedApiDelegate instance.");
            return false;
        }
        return delegate.sendShowRestrictedContentIntent(uri);
    }

    public static void setDelegateForTesting(@Nullable AconfigFlaggedApiDelegate delegate) {
        sDelegateSupplierForTesting = () -> delegate;
        ResettersForTesting.register(() -> sDelegateSupplierForTesting = null);
    }

    public static void setParcelFileDescriptorPipeFactoryForTesting(
            ParcelFileDescriptorPipeFactory factory) {
        sPipeFactory = factory;
    }
}
