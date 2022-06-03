// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;

/**
 * Provides DisplayRefreshRate tracking for MainBeginFrameSourceWebView
 */
@JNINamespace("android_webview")
public class RootBeginFrameSourceWebView implements DisplayAndroidObserver {
    private long mNativeRootBeginFrameSourceWebView;
    private final DisplayAndroid mDisplayAndroid;

    @CalledByNative
    private RootBeginFrameSourceWebView(long nativeRootBeginFrameSourceWebView) {
        mNativeRootBeginFrameSourceWebView = nativeRootBeginFrameSourceWebView;

        mDisplayAndroid = DisplayAndroid.getNonMultiDisplay(ContextUtils.getApplicationContext());
        mDisplayAndroid.addObserver(this);
        onRefreshRateChanged(mDisplayAndroid.getRefreshRate());
    }

    @Override
    public void onRefreshRateChanged(float refreshRate) {
        RootBeginFrameSourceWebViewJni.get().onUpdateRefreshRate(
                mNativeRootBeginFrameSourceWebView, RootBeginFrameSourceWebView.this, refreshRate);
    }

    @NativeMethods
    interface Natives {
        void onUpdateRefreshRate(long nativeRootBeginFrameSourceWebView,
                RootBeginFrameSourceWebView caller, float refreshRate);
    }
};