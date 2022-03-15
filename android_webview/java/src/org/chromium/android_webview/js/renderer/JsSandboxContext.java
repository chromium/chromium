// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.renderer;

import android.os.Handler;
import android.os.Looper;
import android.os.RemoteException;

import org.chromium.android_webview.js.common.IJsSandboxContext;
import org.chromium.android_webview.js.common.IJsSandboxContextCallback;
import org.chromium.base.Log;

/**
 * Service that provides a method for Javascript execution. TODO(crbug.com/1297672): Currently this
 * is just meant to be a tracer to define the end to end flow and does not do anything useful.
 */
public class JsSandboxContext extends IJsSandboxContext.Stub {
    private static final String TAG = "JsSandboxContext";
    private boolean mIsClosed = false;

    @Override
    public void evaluateJavascript(String code, IJsSandboxContextCallback callback) {
        if (mIsClosed) {
            throw new IllegalStateException("evaluateJavascript() called after close()");
        }
        // Just posting to the mainLooper for now.
        Handler handler = new Handler(Looper.getMainLooper());
        handler.postDelayed(() -> {
            // Currently just hardcoded to return error string when it encounters "ERROR" as input.
            if (code.equals("ERROR")) {
                try {
                    callback.reportError("There has been an error.");
                } catch (RemoteException e) {
                    Log.e(TAG, "reporting result failed", e);
                }
            } else {
                String result = code.toUpperCase();
                try {
                    callback.reportResult(result);
                } catch (RemoteException e) {
                    Log.e(TAG, "reporting result failed", e);
                }
            }
        }, 100);
    }

    @Override
    public void close() {
        mIsClosed = true;
        // Do nothing for now. Eventually this should stop the execution and destroy the
        // context in native.
    }
}
