// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.WebViewRenderProcess;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.base.annotations.VerifiesOnQ;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
class WebViewRenderProcessAdapter extends WebViewRenderProcess {
    private static WeakHashMap<AwRenderProcess, WebViewRenderProcessAdapter> sInstances =
            new WeakHashMap<>();

    private WeakReference<AwRenderProcess> mAwRenderProcessWeakRef;

    public static WebViewRenderProcessAdapter getInstanceFor(AwRenderProcess awRenderProcess) {
        if (awRenderProcess == null) {
            return null;
        }
        WebViewRenderProcessAdapter instance = sInstances.get(awRenderProcess);
        if (instance == null) {
            sInstances.put(
                    awRenderProcess, instance = new WebViewRenderProcessAdapter(awRenderProcess));
        }
        return instance;
    }

    private WebViewRenderProcessAdapter(AwRenderProcess awRenderProcess) {
        mAwRenderProcessWeakRef = new WeakReference<>(awRenderProcess);
    }

    @Override
    @SuppressLint("Override")
    public boolean terminate() {
        AwRenderProcess renderer = mAwRenderProcessWeakRef.get();
        if (renderer == null) {
            return false;
        }
        return renderer.terminate();
    }
}