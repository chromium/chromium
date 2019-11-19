// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.webkit.WebView;

import java.lang.reflect.InvocationTargetException;

/**
 * A helper class to get info about WebView package.
 */
// TODO(crbug.com/1020024) use androidx.webkit.WebViewCompat#getCurrentWebViewPackage and remove
// this class.
public final class WebViewPackageHelper {
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
    public static PackageInfo getCurrentWebViewPackage(Context context) {
        // There was no WebView Package before Lollipop, the WebView code was part of the framework
        // back then.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return null;
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
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                    && Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
                Class<?> webViewFactoryClass = Class.forName("android.webkit.WebViewFactory");

                webviewPackageName = (String) webViewFactoryClass.getMethod("getWebViewPackageName")
                                             .invoke(null);
            } else {
                Class<?> webviewUpdateServiceClass =
                        Class.forName("android.webkit.WebViewUpdateService");
                webviewPackageName =
                        (String) webviewUpdateServiceClass.getMethod("getCurrentWebViewPackageName")
                                .invoke(null);
            }
        } catch (ClassNotFoundException e) {
            return null;
        } catch (IllegalAccessException e) {
            return null;
        } catch (InvocationTargetException e) {
            return null;
        } catch (NoSuchMethodException e) {
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

    // Do not instantiate this class.
    private WebViewPackageHelper() {}
}
