// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.nonembedded_util;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.webkit.WebView;

import org.chromium.base.ResettersForTesting;

import java.util.Locale;

/** A helper class to get info about WebView package. */
public final class WebViewPackageHelper {
    private static PackageInfo sWebViewCurrentPackageForTesting;

    /** Wrapper to mock WebView.getCurrentWebViewPackage() for testing. */
    public static PackageInfo getCurrentWebViewPackage(Context context) {
        if (sWebViewCurrentPackageForTesting != null) {
            return sWebViewCurrentPackageForTesting;
        }
        return WebView.getCurrentWebViewPackage();
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

    /** Check if the system currently has a valid WebView implementation. */
    public static boolean hasValidWebViewImplementation(Context context) {
        return getCurrentWebViewPackage(context) != null;
    }

    /**
     * Loads a label for the app specified by {@code context}. This is designed to be consistent
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
