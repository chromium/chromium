// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.permission;

import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.CleanupReference;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.Nullable;

/** This class wraps permission request in Chromium side, and can only be created by native side. */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class AwPermissionRequest {
    @JniType("::android_webview::AwPermissionRequest")
    interface NativeAwPermissionRequest extends JniTypeToken {}

    private final Uri mOrigin;
    private final long mResources;
    private boolean mProcessed;

    // AwPermissionRequest native instance.
    private @Nullable JniUniquePtr<NativeAwPermissionRequest> mNativeAwPermissionRequest;

    // Responsible for deleting native peer.
    private @Nullable CleanupReference mCleanupReference;

    @CalledByNative
    static AwPermissionRequest create(
            JniUniquePtr<NativeAwPermissionRequest> nativeAwPermissionRequest,
            @JniType("std::string") String url,
            long resources) {
        return new AwPermissionRequest(nativeAwPermissionRequest, Uri.parse(url), resources);
    }

    private AwPermissionRequest(
            JniUniquePtr<NativeAwPermissionRequest> nativeAwPermissionRequest,
            Uri origin,
            long resources) {
        mNativeAwPermissionRequest = nativeAwPermissionRequest;
        mOrigin = origin;
        mResources = resources;
        mCleanupReference = new CleanupReference(this, (e) -> nativeAwPermissionRequest.destroy());
    }

    public Uri getOrigin() {
        return mOrigin;
    }

    public long getResources() {
        return mResources;
    }

    public void grant() {
        validate();
        if (mNativeAwPermissionRequest != null) {
            AwPermissionRequestJni.get().onAccept(mNativeAwPermissionRequest, true);
            destroyNative();
        }
        mProcessed = true;
    }

    public void deny() {
        validate();
        if (mNativeAwPermissionRequest != null) {
            AwPermissionRequestJni.get().onAccept(mNativeAwPermissionRequest, false);
            destroyNative();
        }
        mProcessed = true;
    }

    @CalledByNative
    private void destroyNative() {
        mNativeAwPermissionRequest = null;
        if (mCleanupReference != null) {
            mCleanupReference.cleanupNow();
            mCleanupReference = null;
        }
    }

    private void validate() {
        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException(
                    "Either grant() or deny() should be called on UI thread");
        }

        if (mProcessed) {
            throw new IllegalStateException("Either grant() or deny() has been already called.");
        }
    }

    @NativeMethods
    interface Natives {
        void onAccept(JniPtr<NativeAwPermissionRequest> self, boolean allowed);
    }
}
