// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Locale;

/** Convenience wrapper for parameters to {@link HostBrowserLauncher} methods. */
public class HostBrowserLauncherParams {
    private String mHostBrowserPackageName;
    private int mHostBrowserMajorChromiumVersion;
    private boolean mDialogShown;
    private Intent mOriginalIntent;
    private String mStartUrl;
    private int mSource;
    private boolean mForceNavigation;
    private long mLaunchTimeMs;
    private long mSplashShownTimeMs;
    private String mSelectedShareTargetActivityClassName;

    /**
     * Constructs a HostBrowserLauncherParams object from the passed in Intent and from <meta-data>
     * in the Android Manifest.
     */
    public static HostBrowserLauncherParams createForIntent(Context context, Intent intent,
            String hostBrowserPackageName, boolean dialogShown, long launchTimeMs,
            long splashShownTimeMs) {
        Bundle metadata = WebApkUtils.readMetaData(context);
        if (metadata == null) return null;

        int hostBrowserMajorChromiumVersion = HostBrowserUtils.queryHostBrowserMajorChromiumVersion(
                context, hostBrowserPackageName);
        long intentLaunchTimeMs = intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
        if (intentLaunchTimeMs > 0) {
            launchTimeMs = intentLaunchTimeMs;
        }

        String startUrl = null;
        int source = WebApkConstants.SHORTCUT_SOURCE_UNKNOWN;
        boolean forceNavigation = false;

        // If the intent was from the WebAPK relaunching itself or from the host browser relaunching
        // the WebAPK via {@link H2OLauncher#requestRelaunchFromHostBrowser()}, we cannot determine
        // whether the intent is a share intent from the intent's action.
        String selectedShareTargetActivityClassName = intent.getStringExtra(
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);

        if (Intent.ACTION_SEND.equals(intent.getAction())
                || Intent.ACTION_SEND_MULTIPLE.equals(intent.getAction())) {
            selectedShareTargetActivityClassName = intent.getComponent().getClassName();
        }

        if (selectedShareTargetActivityClassName != null) {
            Bundle shareTargetMetaData = fetchActivityMetaData(context,
                    new ComponentName(
                            context.getPackageName(), selectedShareTargetActivityClassName));
            startUrl = computeStartUrlForShareTarget(shareTargetMetaData, intent);
            source = WebApkConstants.SHORTCUT_SOURCE_SHARE;
            forceNavigation = true;
        } else if (!TextUtils.isEmpty(intent.getDataString())) {
            startUrl = intent.getDataString();
            source = intent.getIntExtra(
                    WebApkConstants.EXTRA_SOURCE, WebApkConstants.SHORTCUT_SOURCE_EXTERNAL_INTENT);
            forceNavigation = intent.getBooleanExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, true);
        } else {
            startUrl = metadata.getString(WebApkMetaDataKeys.START_URL);
            source = WebApkConstants.SHORTCUT_SOURCE_UNKNOWN;
            forceNavigation = false;
        }

        if (startUrl == null) return null;

        startUrl = WebApkUtils.rewriteIntentUrlIfNecessary(startUrl, metadata);

        // Ignore deep links which came with non HTTP/HTTPS schemes and which were not rewritten.
        if (!doesUrlUseHttpOrHttpsScheme(startUrl)) return null;

        return new HostBrowserLauncherParams(hostBrowserPackageName,
                hostBrowserMajorChromiumVersion, dialogShown, intent, startUrl, source,
                forceNavigation, launchTimeMs, splashShownTimeMs,
                selectedShareTargetActivityClassName);
    }

    private static Bundle fetchActivityMetaData(
            Context context, ComponentName shareTargetComponentName) {
        ActivityInfo shareActivityInfo;
        try {
            shareActivityInfo = context.getPackageManager().getActivityInfo(
                    shareTargetComponentName, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        if (shareActivityInfo == null) {
            return null;
        }
        return shareActivityInfo.metaData;
    }

    private static boolean doesShareTargetUsePost(Bundle shareTargetMetaData) {
        String method = shareTargetMetaData.getString(WebApkMetaDataKeys.SHARE_METHOD);
        if (TextUtils.isEmpty(method)) {
            return false;
        }
        return "POST".equals(method.toUpperCase(Locale.ENGLISH));
    }

    /**
     * Computes the start URL for the given share intent and share activity.
     * @param shareTargetMetaData Meta data for the share target activity selected by the user.
     * @param intent Share intent.
     */
    protected static String computeStartUrlForShareTarget(
            Bundle shareTargetMetaData, Intent intent) {
        if (shareTargetMetaData == null) {
            return null;
        }
        if (doesShareTargetUsePost(shareTargetMetaData)) {
            return shareTargetMetaData.getString(WebApkMetaDataKeys.SHARE_ACTION);
        }
        return computeStartUrlForGETShareTarget(shareTargetMetaData, intent);
    }

    /**
     * Computes the start URL for the given share intent and share activity which sends GET HTTP
     * requests.
     * @param shareTargetMetaData Meta data for the share target activity selected by the user.
     * @param intent Share intent.
     */

    private static String computeStartUrlForGETShareTarget(
            Bundle shareTargetMetaData, Intent intent) {
        String shareAction = shareTargetMetaData.getString(WebApkMetaDataKeys.SHARE_ACTION);
        if (TextUtils.isEmpty(shareAction)) {
            return null;
        }

        // These can be null, they are checked downstream.
        ArrayList<Pair<String, String>> entryList = new ArrayList<>();
        entryList.add(
                new Pair<>(shareTargetMetaData.getString(WebApkMetaDataKeys.SHARE_PARAM_TITLE),
                        intent.getStringExtra(Intent.EXTRA_SUBJECT)));
        entryList.add(new Pair<>(shareTargetMetaData.getString(WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                intent.getStringExtra(Intent.EXTRA_TEXT)));

        return createGETWebShareTargetUriString(shareAction, entryList);
    }

    /**
     * Converts the action url and parameters of a GET webshare target into a URI.
     * Example:
     * - action = "https://example.org/includinator/share.html"
     * - params
     *     title param: "title"
     *     title intent: "news"
     *     text param: "description"
     *     text intent: "story"
     * Becomes:
     *   https://example.org/includinator/share.html?title=news&description=story
     * TODO(ckitagawa): The escaping behavior isn't entirely correct. The exact encoding is still
     * being discussed at https://github.com/WICG/web-share-target/issues/59.
     */
    protected static String createGETWebShareTargetUriString(
            String action, ArrayList<Pair<String, String>> entryList) {
        Uri.Builder queryBuilder = new Uri.Builder();
        for (Pair<String, String> nameValue : entryList) {
            if (!TextUtils.isEmpty(nameValue.first) && !TextUtils.isEmpty(nameValue.second)) {
                // Uri.Builder does URL escaping.
                queryBuilder.appendQueryParameter(nameValue.first, nameValue.second);
            }
        }
        Uri shareUri = Uri.parse(action);
        Uri.Builder builder = shareUri.buildUpon();
        // Uri.Builder uses %20 rather than + for spaces, the spec requires +.
        String queryString = queryBuilder.build().toString();
        if (TextUtils.isEmpty(queryString)) {
            return action;
        }
        builder.encodedQuery(queryString.replace("%20", "+").substring(1));
        return builder.build().toString();
    }

    /** Returns whether the URL uses the HTTP or HTTPs schemes. */
    private static boolean doesUrlUseHttpOrHttpsScheme(String url) {
        return url != null && (url.startsWith("http:") || url.startsWith("https:"));
    }

    private HostBrowserLauncherParams(String hostBrowserPackageName,
            int hostBrowserMajorChromiumVersion, boolean dialogShown, Intent originalIntent,
            String startUrl, int source, boolean forceNavigation, long launchTimeMs,
            long splashShownTimeMs, String selectedShareTargetActivityClassName) {
        mHostBrowserPackageName = hostBrowserPackageName;
        mHostBrowserMajorChromiumVersion = hostBrowserMajorChromiumVersion;
        mDialogShown = dialogShown;
        mOriginalIntent = originalIntent;
        mStartUrl = startUrl;
        mSource = source;
        mForceNavigation = forceNavigation;
        mLaunchTimeMs = launchTimeMs;
        mSplashShownTimeMs = splashShownTimeMs;
        mSelectedShareTargetActivityClassName = selectedShareTargetActivityClassName;
    }

    /** Returns the chosen host browser. */
    public String getHostBrowserPackageName() {
        return mHostBrowserPackageName;
    }

    /**
     * Returns the major version of the host browser. Currently, only Chromium host browsers
     * (Chrome Canary, Chrome Dev ...) are supported.
     */
    public int getHostBrowserMajorChromiumVersion() {
        return mHostBrowserMajorChromiumVersion;
    }

    /** Returns whether the choose-host-browser dialog was shown. */
    public boolean wasDialogShown() {
        return mDialogShown;
    }

    /** Returns intent used to launch WebAPK. */
    public Intent getOriginalIntent() {
        return mOriginalIntent;
    }

    /** Returns URL to launch WebAPK at. */
    public String getStartUrl() {
        return mStartUrl;
    }

    /** Returns the source which is launching/navigating the WebAPK. */
    public int getSource() {
        return mSource;
    }

    /**
     * Returns whether the WebAPK should be navigated to {@link mStartUrl} if it is already
     * running.
     */
    public boolean getForceNavigation() {
        return mForceNavigation;
    }

    /**
     * If this object was created as a result of launching the WebAPK, returns the time in
     * milliseconds that the WebAPK was launched. If this object was created when the WebAPK was
     * already running (e.g. {@link Activity#onNewIntent()}) returns -1.
     */
    public long getLaunchTimeMs() {
        return mLaunchTimeMs;
    }

    /**
     * Returns timestamp that the splash screen was shown. Returns -1 if the splash screen is not
     * shown by the ShellAPK.
     */
    public long getSplashShownTimeMs() {
        return mSplashShownTimeMs;
    }

    /** Returns the class name of the share activity that the user selected. */
    public String getSelectedShareTargetActivityClassName() {
        return mSelectedShareTargetActivityClassName;
    }
}
