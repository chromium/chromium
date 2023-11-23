// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.permission;

import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.CleanupReference;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;

/**
 * This class wraps permission request in Chromium side, and can only be created
 * by native side.
 */
@Lifetime.Temporary
@JNINamespace("android_webview")
public class AwPermissionRequest {
    private final Uri mOrigin;
    private final long mResources;
    private boolean mProcessed;

    // AwPermissionRequest native instance.
    private long mNativeAwPermissionRequest;

    // Responsible for deleting native peer.
    private CleanupReference mCleanupReference;

    private static final class DestroyRunnable implements Runnable {
        private final long mNativeAwPermissionRequest;

        private DestroyRunnable(long nativeAwPermissionRequest) {
            mNativeAwPermissionRequest = nativeAwPermissionRequest;
        }

        @Override
        public void run() {
            AwPermissionRequestJni.get().destroy(mNativeAwPermissionRequest);
        }
    }

    @CalledByNative
    private static AwPermissionRequest create(
            long nativeAwPermissionRequest, String url, long resources) {
        if (nativeAwPermissionRequest == 0) return null;
        Uri origin = Uri.parse(url);
        return new AwPermissionRequest(nativeAwPermissionRequest, origin, resources);
    }

    private AwPermissionRequest(long nativeAwPermissionRequest, Uri origin, long resources) {
        mNativeAwPermissionRequest = nativeAwPermissionRequest;
        mOrigin = origin;
        mResources = resources;
        mCleanupReference =
                new CleanupReference(this, new DestroyRunnable(mNativeAwPermissionRequest));
    }

    public Uri getOrigin() {
        return mOrigin;
    }

    public long getResources() {
        return mResources;
    }

    public void grant() {
        validate();
        if (mNativeAwPermissionRequest != 0) {
            AwPermissionRequestJni.get()
                    .onAccept(mNativeAwPermissionRequest, AwPermissionRequest.this, true);
            destroyNative();
        }
        mProcessed = true;
    }

    public void deny() {
        validate();
        if (mNativeAwPermissionRequest != 0) {
            AwPermissionRequestJni.get()
                    .onAccept(mNativeAwPermissionRequest, AwPermissionRequest.this, false);
            destroyNative();
        }
        mProcessed = true;
    }

    @CalledByNative
    private void destroyNative() {
        mCleanupReference.cleanupNow();
        mCleanupReference = null;
        mNativeAwPermissionRequest = 0;
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
        void onAccept(long nativeAwPermissionRequest, AwPermissionRequest caller, boolean allowed);

        void destroy(long nativeAwPermissionRequest);
    }
}
