// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.permission;

import android.net.Uri;

import org.chromium.android_webview.CleanupReference;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * This class wraps permission request in Chromium side, and can only be created
 * by native side.
 */
@JNINamespace("android_webview")
public class AwPermissionRequest {
    private final Uri mOrigin;
    private final long mResources;
    private boolean mProcessed;

    // AwPermissionRequest native instance.
    private long mNativeAwPermissionRequest;

    // Responsible for deleting native peer.
    private CleanupReference mCleanupReference;

    // This should be same as corresponding definition in PermissionRequest.
    // We duplicate definition because it is used in Android L and afterwards, but is only
    // defined in M.
    // TODO(michaelbai) : Replace "android.webkit.resource.MIDI_SYSEX" with
    // PermissionRequest.RESOURCE_MIDI_SYSEX once Android M SDK is used.
    public static final String RESOURCE_MIDI_SYSEX = "android.webkit.resource.MIDI_SYSEX";

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
    private static AwPermissionRequest create(long nativeAwPermissionRequest, String url,
            long resources) {
        if (nativeAwPermissionRequest == 0) return null;
        Uri origin = Uri.parse(url);
        return new AwPermissionRequest(nativeAwPermissionRequest, origin, resources);
    }

    private AwPermissionRequest(long nativeAwPermissionRequest, Uri origin,
            long resources) {
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
            AwPermissionRequestJni.get().onAccept(
                    mNativeAwPermissionRequest, AwPermissionRequest.this, true);
            destroyNative();
        }
        mProcessed = true;
    }

    public void deny() {
        validate();
        if (mNativeAwPermissionRequest != 0) {
            AwPermissionRequestJni.get().onAccept(
                    mNativeAwPermissionRequest, AwPermissionRequest.this, false);
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
