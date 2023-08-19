// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebViewFactory;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.library_loader.NativeLibraryPreloader;

/**
 * The library preloader for Monochrome and Trichrome for sharing native library's relro
 * between Chrome and WebView/WebLayer.
 */
@Lifetime.Singleton
public class WebViewLibraryPreloader extends NativeLibraryPreloader {
    @Override
    public int loadLibrary(String packageName) {
        return WebViewFactory.loadWebViewNativeLibraryFromPackage(
                packageName, getClass().getClassLoader());
    }
}
