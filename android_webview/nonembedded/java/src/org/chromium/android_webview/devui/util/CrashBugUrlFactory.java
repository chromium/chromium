// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import android.content.Intent;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.BugTrackerConstants;
import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.base.ContextUtils;

import java.util.Locale;

/** Factory class to build bug URI for a crash report. */
public class CrashBugUrlFactory {
    // There is a limit on the length of this query string, see https://crbug.com/1015923
    // TODO(crbug.com/40674230): add assert statement to check the length of this String.
    @VisibleForTesting
    public static final String CRASH_REPORT_TEMPLATE =
            """
            Basic info:

            * Build fingerprint: %s
            * Android API level: %s
            * Crashed WebView version: %s
            * DevTools version: %s
            * Application: %s
            * If this app is available on Google Play, please include a URL:

            Steps to reproduce:

            1.
            2.
            3.

            Expected result:
            (What should have happened?)

            <Any additional comments, you want to share>"

            ---

            **DO NOT CHANGE BELOW THIS LINE**

            * Crash ID: http://crash/%s
            * Instructions for triaging this report (Chromium members only): https://bit.ly/2SM1Y9t
            """;

    private final CrashInfo mCrashInfo;

    public CrashBugUrlFactory(CrashInfo crashInfo) {
        mCrashInfo = crashInfo;
    }

    /** Build an {@link Intent} to open a URL to report the crash bug in a browser. */
    public Intent getReportIntent() {
        return new Intent(Intent.ACTION_VIEW, getReportUri());
    }

    /**
     * Build a report uri to open an issue on
     * https://issues.chromium.org/issues/new?component=1456456&template=1923373. It uses WebView
     * Bugs Template and overrides labels and description fields.
     */
    public Uri getReportUri() {
        var builder =
                new Uri.Builder()
                        .scheme("https")
                        .authority("issues.chromium.org")
                        .path("/issues/new")
                        .appendQueryParameter(
                                "component", BugTrackerConstants.COMPONENT_MOBILE_WEBVIEW)
                        .appendQueryParameter(
                                "template", BugTrackerConstants.DEFAULT_WEBVIEW_TEMPLATE)
                        .appendQueryParameter("description", getDescription())
                        .appendQueryParameter("priority", "P3")
                        .appendQueryParameter("type", "BUG")
                        .appendQueryParameter(
                                "customFields", BugTrackerConstants.OS_FIELD + ":Android")
                        .appendQueryParameter(
                                "hotlistIds", BugTrackerConstants.USER_SUBMITTED_HOTLIST);

        String version = mCrashInfo.getCrashKey(CrashInfo.WEBVIEW_VERSION_KEY);
        if (version != null) {
            int firstIndex = version.indexOf(".");
            if (firstIndex != -1) {
                String milestone = version.substring(0, firstIndex);
                builder.appendQueryParameter("foundIn", milestone);
            }
        }
        return builder.build();
    }

    // Construct bug description using CRASH_REPORT_TEMPLATE
    private String getDescription() {
        return String.format(
                Locale.US,
                CRASH_REPORT_TEMPLATE,
                Build.FINGERPRINT,
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.ANDROID_SDK_INT_KEY, ""),
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.WEBVIEW_VERSION_KEY, ""),
                getCurrentDevToolsVersion(),
                getCrashAppPackageInfo(),
                mCrashInfo.uploadId);
    }

    // Current Developer UI version and package name which can be different from crash's.
    @VisibleForTesting
    public static String getCurrentDevToolsVersion() {
        PackageInfo webViewPackage =
                WebViewPackageHelper.getContextPackageInfo(ContextUtils.getApplicationContext());
        return String.format(
                Locale.US,
                "%s (%s/%s)",
                webViewPackage.packageName,
                webViewPackage.versionName,
                webViewPackage.versionCode);
    }

    // The package name and version code of the app where WebView crashed.
    private String getCrashAppPackageInfo() {
        String name = mCrashInfo.getCrashKeyOrDefault(CrashInfo.APP_PACKAGE_NAME_KEY, "");
        String versionCode =
                mCrashInfo.getCrashKeyOrDefault(CrashInfo.APP_PACKAGE_VERSION_CODE_KEY, "");
        return String.format(Locale.US, "%s (%s)", name, versionCode);
    }
}
