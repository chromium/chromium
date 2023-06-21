// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded_util;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.webkit.WebView;

import org.chromium.base.ResettersForTesting;

import java.lang.reflect.InvocationTargetException;
import java.util.Locale;

/**
 * A helper class to get info about WebView package.
 */
public final class WebViewPackageHelper {
    private static PackageInfo sWebViewCurrentPackageForTesting;

    /**
     * If WebView has already been loaded into the current process this method will return the
     * package that was used to load it. Otherwise, the package that would be used if the WebView
     * was loaded right now will be returned; this does not cause WebView to be loaded, so this
     * information may become outdated at any time.
     * The WebView package changes either when the current WebView package is updated, disabled, or
     * uninstalled. It can also be changed through a Developer Setting.
     * If the WebView package changes, any app process that has loaded WebView will be killed. The
     * next time the app starts and loads WebView it will use the new WebView package instead.
     * @return the current WebView package, or {@code null} if there is none.
     */
    // This method is copied from androidx.webkit.WebViewCompat.
    // TODO(crbug.com/1020024) use androidx.webkit.WebViewCompat#getCurrentWebViewPackage instead.
    @SuppressWarnings("WebViewApiAvailability")
    public static PackageInfo getCurrentWebViewPackage(Context context) {
        if (sWebViewCurrentPackageForTesting != null) {
            return sWebViewCurrentPackageForTesting;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return WebView.getCurrentWebViewPackage();
        } else { // L-N
            try {
                PackageInfo loadedWebViewPackageInfo = getLoadedWebViewPackageInfo();
                if (loadedWebViewPackageInfo != null) return loadedWebViewPackageInfo;
            } catch (ClassNotFoundException | IllegalAccessException | InvocationTargetException
                    | NoSuchMethodException e) {
                return null;
            }

            // If WebViewFactory.getLoadedPackageInfo() returns null then WebView hasn't been loaded
            // yet, in that case we need to fetch the name of the WebView package, and fetch the
            // corresponding PackageInfo through the PackageManager
            return getNotYetLoadedWebViewPackageInfo(context);
        }
    }

    /**
     * Return the PackageInfo of the currently loaded WebView APK. This method uses reflection and
     * propagates any exceptions thrown, to the caller.
     */
    // This method is copied from androidx.webkit.WebViewCompat.
    @SuppressLint("PrivateApi")
    private static PackageInfo getLoadedWebViewPackageInfo()
            throws ClassNotFoundException, NoSuchMethodException, InvocationTargetException,
                   IllegalAccessException {
        Class<?> webViewFactoryClass = Class.forName("android.webkit.WebViewFactory");
        return (PackageInfo) webViewFactoryClass.getMethod("getLoadedPackageInfo").invoke(null);
    }

    /**
     * Return the PackageInfo of the WebView APK that would have been used as WebView implementation
     * if WebView was to be loaded right now.
     */
    // This method is copied from androidx.webkit.WebViewCompat.
    @SuppressLint("PrivateApi")
    private static PackageInfo getNotYetLoadedWebViewPackageInfo(Context context) {
        String webviewPackageName;
        try {
            Class<?> webviewUpdateServiceClass =
                    Class.forName("android.webkit.WebViewUpdateService");
            webviewPackageName =
                    (String) webviewUpdateServiceClass.getMethod("getCurrentWebViewPackageName")
                            .invoke(null);
        } catch (Exception e) {
            return null;
        }
        if (webviewPackageName == null) return null;
        PackageManager pm = context.getPackageManager();
        try {
            return pm.getPackageInfo(webviewPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Get {@link PackageInfo} for the given {@link Context}.
     * It uses {@link Context#getPackageName()} to look up the {@link PackageInfo} object.
     */
    public static PackageInfo getContextPackageInfo(Context context) {
        try {
            return context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Check if the given context is a WebView package and if it's currently selected as system's
     * WebView implementation.
     *
     * @param context the {@link Context} to check its package.
     */
    public static boolean isCurrentSystemWebViewImplementation(Context context) {
        PackageInfo systemWebViewPackage = getCurrentWebViewPackage(context);
        if (systemWebViewPackage == null) return false;
        return context.getPackageName().equals(systemWebViewPackage.packageName);
    }

    /**
     * Check if the system currently has a valid WebView implementation.
     */
    public static boolean hasValidWebViewImplementation(Context context) {
        return getCurrentWebViewPackage(context) != null;
    }

    /**
     * Loads a label for the app specified by {@code mContext}. This is designed to be consistent
     * with how the system's WebView chooser labels WebView packages (see {@code
     * com.android.settings.webview.WebViewAppPicker.WebViewAppInfo#loadLabel()} in the AOSP source
     * code).
     */
    public static CharSequence loadLabel(Context context) {
        ApplicationInfo applicationInfo = context.getApplicationInfo();
        PackageManager pm = context.getPackageManager();
        CharSequence appLabel = applicationInfo.loadLabel(pm);
        try {
            String versionName = pm.getPackageInfo(context.getPackageName(), 0).versionName;
            return String.format(Locale.US, "%s %s", appLabel, versionName);
        } catch (PackageManager.NameNotFoundException e) {
            return appLabel;
        }
    }

    /**
     * Inject a {@link PackageInfo} as the current webview package for testing. This PackageInfo
     * will be returned by {@link #getCurrentWebViewPackage}.
     */
    public static void setCurrentWebViewPackageForTesting(PackageInfo currentWebView) {
        sWebViewCurrentPackageForTesting = currentWebView;
        ResettersForTesting.register(() -> sWebViewCurrentPackageForTesting = null);
    }

    // Do not instantiate this class.
    private WebViewPackageHelper() {}
}
