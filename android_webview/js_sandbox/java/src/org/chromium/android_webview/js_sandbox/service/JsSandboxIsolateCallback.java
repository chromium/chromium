// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import android.os.RemoteException;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;
import org.chromium.base.Log;

/** Callback interface for the native code to report a JavaScript evaluation outcome. */
@JNINamespace("android_webview")
public class JsSandboxIsolateCallback {
    private static final String TAG = "JsSandboxIsolateCallback";

    private final IJsSandboxIsolateCallback mCallback;

    JsSandboxIsolateCallback(IJsSandboxIsolateCallback callback) {
        mCallback = callback;
    }

    /**
     * Called when an evaluation succeeds immediately or after its promise resolves.
     *
     * @param result The string result of the evaluation or resolved evaluation promise.
     */
    @CalledByNative
    public void onResult(String result) {
        try {
            mCallback.reportResult(result);
        } catch (RemoteException e) {
            Log.e(TAG, "reporting result failed", e);
        }
    }

    /**
     * Called in the event of an error.
     *
     * @param errorType See
     *                  {@link
     * org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback} for error types.
     * @param error     String description of the error.
     */
    @CalledByNative
    public void onError(int errorType, String error) {
        try {
            mCallback.reportError(errorType, error);
        } catch (RemoteException e) {
            Log.e(TAG, "reporting error failed", e);
        }
    }
}
