// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.content_public.browser.WebContents;

/** The class to handle dark mode. */
@Lifetime.WebView
@JNINamespace("android_webview")
public class AwDarkMode {
    private Context mContext;
    private long mNativeAwDarkMode;

    private static boolean sEnableSimplifiedDarkMode;

    public static void enableSimplifiedDarkMode() {
        sEnableSimplifiedDarkMode = true;
        AwDarkModeJni.get().enableSimplifiedDarkMode();
    }

    public AwDarkMode(Context context) {
        mContext = context;
    }

    public void setWebContents(WebContents webContents) {
        if (mNativeAwDarkMode != 0) {
            AwDarkModeJni.get().detachFromJavaObject(mNativeAwDarkMode, this);
            mNativeAwDarkMode = 0;
        }
        if (webContents != null) {
            mNativeAwDarkMode = AwDarkModeJni.get().init(this, webContents);
        }
    }

    public static boolean isSimplifiedDarkModeEnabled() {
        return sEnableSimplifiedDarkMode;
    }

    public void destroy() {
        setWebContents(null);
    }

    @CalledByNative
    private boolean isAppUsingDarkTheme() {
        return DarkModeHelper.LightTheme.LIGHT_THEME_FALSE
                == DarkModeHelper.getLightTheme(mContext);
    }

    @CalledByNative
    private void onNativeObjectDestroyed() {
        mNativeAwDarkMode = 0;
    }

    @NativeMethods
    interface Natives {
        void enableSimplifiedDarkMode();

        long init(AwDarkMode caller, WebContents webContents);

        void detachFromJavaObject(long nativeAwDarkMode, AwDarkMode caller);
    }
}
