// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.webkit.WebViewRenderProcess;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.common.Lifetime;

import java.lang.ref.WeakReference;
import java.util.WeakHashMap;

@Lifetime.Renderer
class WebViewRenderProcessAdapter extends WebViewRenderProcess {
    private static final WeakHashMap<AwRenderProcess, WebViewRenderProcessAdapter> sInstances =
            new WeakHashMap<>();

    private final WeakReference<AwRenderProcess> mAwRenderProcessWeakRef;

    public static WebViewRenderProcessAdapter getInstanceFor(AwRenderProcess awRenderProcess) {
        if (awRenderProcess == null) {
            return null;
        }
        WebViewRenderProcessAdapter instance = sInstances.get(awRenderProcess);
        if (instance == null) {
            instance = new WebViewRenderProcessAdapter(awRenderProcess);
            sInstances.put(awRenderProcess, instance);
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
