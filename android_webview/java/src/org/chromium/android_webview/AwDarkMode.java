// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * The class to handle dark mode.
 */
@JNINamespace("android_webview")
public class AwDarkMode {
    private Context mContext;
    private long mNativeAwDarkMode;

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
        long init(AwDarkMode caller, WebContents webContents);
        void detachFromJavaObject(long nativeAwDarkMode, AwDarkMode caller);
    }
}
