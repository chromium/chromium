// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.content.res.AssetFileDescriptor;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateSyncCallback;
import org.chromium.base.Log;

import java.io.IOException;

/** Callback for the native code to report a JavaScript evaluation outcome using FDs. */
@JNINamespace("android_webview")
public class JsSandboxIsolateFdCallback {
    private static final String TAG = "JsSandboxIsolateFdCallback";

    private final IJsSandboxIsolateSyncCallback mCallback;

    JsSandboxIsolateFdCallback(IJsSandboxIsolateSyncCallback callback) {
        mCallback = callback;
    }

    /**
     * Called when an evaluation succeeds immediately or after its promise resolves.
     *
     * @param fd     The fd to which the result of the evaluation or resolved evaluation promise is
     *               written into.
     * @param length Number of bytes written into the fd.
     */
    @CalledByNative
    public void onResult(int fd, int length) {
        try (ParcelFileDescriptor parcelFileDescriptor = ParcelFileDescriptor.adoptFd(fd)) {
            AssetFileDescriptor assetFileDescriptor =
                    new AssetFileDescriptor(parcelFileDescriptor, 0, length);
            mCallback.reportResultWithFd(assetFileDescriptor);
        } catch (RemoteException | IOException e) {
            Log.e(TAG, "reporting result failed", e);
        }
    }

    /**
     * Called in the event of an error.
     *
     * @param errorType See
     *                  {@link
     *                  org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateSyncCallback}
     * for error types.
     * @param fd        The fd to which the description of the error is written into.
     * @param length    Number of bytes written into the fd.
     */
    @CalledByNative
    public void onError(int errorType, int fd, int length) {
        try (ParcelFileDescriptor parcelFileDescriptor = ParcelFileDescriptor.adoptFd(fd)) {
            AssetFileDescriptor assetFileDescriptor =
                    new AssetFileDescriptor(parcelFileDescriptor, 0, length);
            mCallback.reportErrorWithFd(errorType, assetFileDescriptor);
        } catch (RemoteException | IOException e) {
            Log.e(TAG, "reporting error failed", e);
        }
    }
}
