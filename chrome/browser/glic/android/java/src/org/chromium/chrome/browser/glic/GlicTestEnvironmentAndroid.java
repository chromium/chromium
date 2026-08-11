// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.pm.ActivityInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Java wrapper for C++ GlicTestEnvironment. */
@JNINamespace("glic")
public class GlicTestEnvironmentAndroid {
    private final long mNativeGlicTestEnvironmentAndroid;

    public GlicTestEnvironmentAndroid() {
        mNativeGlicTestEnvironmentAndroid = GlicTestEnvironmentAndroidJni.get().init();
    }

    @CalledByNative
    public static void setActivityOrientation(WebContents webContents, int orientation) {
        if (webContents == null) return;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        var weakActivity = window.getActivity();
        if (weakActivity == null) return;
        Activity activity = weakActivity.get();
        if (activity == null) return;
        activity.setRequestedOrientation(
                orientation == 0
                        ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                        : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    public void destroy() {
        GlicTestEnvironmentAndroidJni.get().destroy(mNativeGlicTestEnvironmentAndroid);
    }

    public String getURL(String path) {
        return GlicTestEnvironmentAndroidJni.get().getURL(mNativeGlicTestEnvironmentAndroid, path);
    }

    public boolean isWebClientConnected() {
        return GlicTestEnvironmentAndroidJni.get()
                .isWebClientConnected(mNativeGlicTestEnvironmentAndroid);
    }

    public WebContents getGuestWebContents() {
        return GlicTestEnvironmentAndroidJni.get()
                .getGuestWebContents(mNativeGlicTestEnvironmentAndroid);
    }

    @NativeMethods
    interface Natives {
        long init();

        void destroy(long nativeGlicTestEnvironmentAndroid);

        String getURL(long nativeGlicTestEnvironmentAndroid, String path);

        boolean isWebClientConnected(long nativeGlicTestEnvironmentAndroid);

        @JniType("content::WebContents*")
        WebContents getGuestWebContents(long nativeGlicTestEnvironmentAndroid);
    }
}
