// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.content.pm.ActivityInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Java wrapper for C++ GlicTestEnvironment. */
@JNINamespace("glic")
public class GlicTestEnvironmentAndroid {
    private final JniRawPtr<NativeGlicTestEnvironmentAndroid> mNativeGlicTestEnvironmentAndroid;

    public GlicTestEnvironmentAndroid() {
        mNativeGlicTestEnvironmentAndroid = GlicTestEnvironmentAndroidJni.get().init();
    }

    @CalledByNative
    public static void setActivityOrientation(
            @JniType("content::WebContents*") @Nullable WebContents webContents, int orientation) {
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
        mNativeGlicTestEnvironmentAndroid.release();
    }

    public String getURL(String path) {
        return GlicTestEnvironmentAndroidJni.get().getURL(mNativeGlicTestEnvironmentAndroid, path);
    }

    public boolean isWebClientConnected() {
        return GlicTestEnvironmentAndroidJni.get()
                .isWebClientConnected(mNativeGlicTestEnvironmentAndroid);
    }

    public @Nullable WebContents getGuestWebContents() {
        return GlicTestEnvironmentAndroidJni.get()
                .getGuestWebContents(mNativeGlicTestEnvironmentAndroid);
    }

    @JniType("::glic::GlicTestEnvironmentAndroid")
    interface NativeGlicTestEnvironmentAndroid extends JniTypeToken {}

    @NativeMethods
    interface Natives {
        JniRawPtr<NativeGlicTestEnvironmentAndroid> init();

        void destroy(JniPtr<NativeGlicTestEnvironmentAndroid> self);

        @JniType("std::string")
        String getURL(
                JniPtr<NativeGlicTestEnvironmentAndroid> self, @JniType("std::string") String path);

        boolean isWebClientConnected(JniPtr<NativeGlicTestEnvironmentAndroid> self);

        @JniType("content::WebContents*")
        @Nullable WebContents getGuestWebContents(JniPtr<NativeGlicTestEnvironmentAndroid> self);
    }
}
