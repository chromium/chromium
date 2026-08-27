// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/** The class to handle dark mode. */
@Lifetime.WebView
@JNINamespace("android_webview")
@NullMarked
public class AwDarkMode {
    private long mNativeAwDarkMode;

    private static boolean sEnableLegacyDarkMode;

    public static void enableLegacyDarkMode() {
        sEnableLegacyDarkMode = true;
        AwDarkModeJni.get().enableLegacyDarkMode();
    }

    public static boolean isLegacyDarkModeEnabled() {
        return sEnableLegacyDarkMode;
    }

    public static void resetForTesting() {
        sEnableLegacyDarkMode = false;
        AwDarkModeJni.get().resetForTesting();
    }

    private final AwContents mAwContents;

    public AwDarkMode(AwContents awContents) {
        mAwContents = awContents;
    }

    public void setWebContents(@Nullable WebContents webContents) {
        if (mNativeAwDarkMode != 0) {
            AwDarkModeJni.get().detachFromJavaObject(mNativeAwDarkMode);
            mNativeAwDarkMode = 0;
        }
        if (webContents != null) {
            mNativeAwDarkMode = AwDarkModeJni.get().init(this, webContents);
        }
    }

    public void destroy() {
        setWebContents(null);
    }

    @CalledByNative
    private boolean isAppUsingDarkTheme() {
        // TODO(b/529634931): We should switch to returning a cached value when we are confident we
        // are not attached to an activity context.
        if (mAwContents != null) {
            return DarkModeHelper.LightTheme.LIGHT_THEME_FALSE
                    == DarkModeHelper.getLightTheme(mAwContents.getProvidedContext());
        }
        return false;
    }

    @CalledByNative
    private void onNativeObjectDestroyed() {
        mNativeAwDarkMode = 0;
    }

    @NativeMethods
    interface Natives {
        void enableLegacyDarkMode();

        void resetForTesting();

        long init(AwDarkMode self, WebContents webContents);

        void detachFromJavaObject(long nativeAwDarkMode);
    }
}
