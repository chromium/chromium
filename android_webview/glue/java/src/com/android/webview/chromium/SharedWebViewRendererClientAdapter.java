// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebView;

import org.chromium.android_webview.AwRenderProcess;

import java.lang.reflect.InvocationHandler;

/**
 */
public class SharedWebViewRendererClientAdapter {
    public SharedWebViewRendererClientAdapter() {}

    public InvocationHandler getSupportLibInvocationHandler() {
        return null;
    }

    public void onRendererUnresponsive(final WebView view, final AwRenderProcess renderProcess) {}

    public void onRendererResponsive(final WebView view, final AwRenderProcess renderProcess) {}
}
