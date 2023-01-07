// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import android.content.Intent;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.base.ContextUtils;

import java.util.Locale;

/**
 * Factory class to build bug URI for a crash report.
 */
public class CrashBugUrlFactory {
    // There is a limit on the length of this query string, see https://crbug.com/1015923
    // TODO(https://crbug.com/1052295): add assert statement to check the length of this String.
    private static final String CRASH_REPORT_TEMPLATE = ""
            + "Build fingerprint: %s\n"
            + "Android API level: %s\n"
            + "Crashed WebView version: %s\n"
            + "DevTools version: %s\n"
            + "Application: %s\n"
            + "If this app is available on Google Play, please include a URL:\n"
            + "\n"
            + "\n"
            + "Steps to reproduce:\n"
            + "(1)\n"
            + "(2)\n"
            + "(3)\n"
            + "\n"
            + "\n"
            + "Expected result:\n"
            + "(What should have happened?)\n"
            + "\n"
            + "\n"
            + "<Any additional comments, you want to share>"
            + "\n"
            + "\n"
            + "****DO NOT CHANGE BELOW THIS LINE****\n"
            + "Crash ID: http://crash/%s\n"
            + "Instructions for triaging this report (Chromium members only): "
            + "https://bit.ly/2SM1Y9t\n";

    private static final String DEFAULT_LABELS =
            "User-Submitted,Via-WebView-DevTools,Pri-3,Type-Bug,OS-Android";

    private final CrashInfo mCrashInfo;

    public CrashBugUrlFactory(CrashInfo crashInfo) {
        mCrashInfo = crashInfo;
    }

    /**
     * Build an {@link Intent} to open a URL to report the crash bug in a browser.
     */
    public Intent getReportIntent() {
        return new Intent(Intent.ACTION_VIEW, getReportUri());
    }

    /**
     * Build a report uri to open an issue on https://bugs.chromium.org/p/chromium/issues/entry.
     * It uses WebView Bugs Template and overrides labels and description fields.
     */
    public Uri getReportUri() {
        return new Uri.Builder()
                .scheme("https")
                .authority("bugs.chromium.org")
                .path("/p/chromium/issues/entry")
                .appendQueryParameter("template", "Webview+Bugs")
                .appendQueryParameter("description", getDescription())
                .appendQueryParameter("labels", getLabels())
                .build();
    }

    // Construct bug description using CRASH_REPORT_TEMPLATE
    private String getDescription() {
        return String.format(Locale.US, CRASH_REPORT_TEMPLATE, Build.FINGERPRINT,
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.ANDROID_SDK_INT_KEY, ""),
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.WEBVIEW_VERSION_KEY, ""),
                getCurrentDevToolsVersion(), getCrashAppPackageInfo(), mCrashInfo.uploadId);
    }

    // Current Developer UI version and package name which can be different from crash's.
    @VisibleForTesting
    public static String getCurrentDevToolsVersion() {
        PackageInfo webViewPackage =
                WebViewPackageHelper.getContextPackageInfo(ContextUtils.getApplicationContext());
        return String.format(Locale.US, "%s (%s/%s)", webViewPackage.packageName,
                webViewPackage.versionName, webViewPackage.versionCode);
    }

    // The package name and version code of the app where WebView crashed.
    private String getCrashAppPackageInfo() {
        String name = mCrashInfo.getCrashKeyOrDefault(CrashInfo.APP_PACKAGE_NAME_KEY, "");
        String versionCode =
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.APP_PACKAGE_VERSION_CODE_KEY, "");
        return String.format(Locale.US, "%s (%s)", name, versionCode);
    }

    // Add FoundIn-<milestone> label to default labels list
    private String getLabels() {
        String labels = DEFAULT_LABELS;
        String version = mCrashInfo.getCrashKey(CrashInfo.WEBVIEW_VERSION_KEY);
        if (version != null) {
            int firstIndex = version.indexOf(".");
            if (firstIndex != -1) {
                String milestone = version.substring(0, firstIndex);
                labels += ",FoundIn-" + milestone;
            }
        }
        return labels;
    }
}