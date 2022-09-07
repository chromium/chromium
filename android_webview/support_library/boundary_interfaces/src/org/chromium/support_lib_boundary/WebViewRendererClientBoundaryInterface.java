// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebView;

import java.lang.reflect.InvocationHandler;

/**
 * Boundary interface for WebViewRendererClient.
 */
public interface WebViewRendererClientBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    void onRendererUnresponsive(WebView view, /* WebViewRenderer */ InvocationHandler renderer);
    void onRendererResponsive(WebView view, /* WebViewRenderer */ InvocationHandler renderer);
}
