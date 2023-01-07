// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.net.Network;
import android.os.Build;
import android.webkit.PacProcessor;

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwPacProcessor;
import org.chromium.base.JNIUtils;
import org.chromium.base.library_loader.LibraryLoader;

@RequiresApi(Build.VERSION_CODES.P)
final class PacProcessorImpl implements PacProcessor {
    static {
        JNIUtils.setClassLoader(WebViewChromiumFactoryProvider.class.getClassLoader());
        LibraryLoader.getInstance().ensureInitialized();

        // This will set up Chromium environment to run proxy resolver.
        AwPacProcessor.initializeEnvironment();
    }

    private static class LazyHolder {
        static final PacProcessorImpl sInstance = new PacProcessorImpl();
    }

    AwPacProcessor mProcessor;

    private PacProcessorImpl() {
        mProcessor = new AwPacProcessor();
    }

    public static PacProcessor getInstance() {
        return LazyHolder.sInstance;
    }

    public static PacProcessor createInstance() {
        return new PacProcessorImpl();
    }

    @Override
    public void release() {
        mProcessor.destroy();
    }

    @Override
    public boolean setProxyScript(String script) {
        return mProcessor.setProxyScript(script);
    }

    @Override
    public String findProxyForUrl(String url) {
        return mProcessor.makeProxyRequest(url);
    }

    @Override
    public void setNetwork(Network network) {
        mProcessor.setNetwork(network);
    }

    @Override
    public Network getNetwork() {
        return mProcessor.getNetwork();
    }
}
