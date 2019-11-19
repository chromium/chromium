// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Map;

/**
 * Stores info for WebAPK.
 */
public class WebApkInfo extends WebappInfo {
    /** A class that stores share information from share intent. */
    public static class ShareData {
        public String subject;
        public String text;
        public ArrayList<Uri> files;
        public String shareActivityClassName;
    }

    /**
     * Stores information about the WebAPK's share intent handlers.
     * TODO(crbug.com/912954): add share target V2 parameters once the server supports them.
     */
    public static class ShareTarget {
        private static final int ACTION_INDEX = 0;
        private static final int PARAM_TITLE_INDEX = 1;
        private static final int PARAM_TEXT_INDEX = 2;
        private String[] mData;
        private boolean mIsShareMethodPost;
        private boolean mIsShareEncTypeMultipart;
        private String[] mFileNames;
        private String[][] mFileAccepts;

        public ShareTarget() {
            this(null, null, null, false, false, null, null);
        }

        public ShareTarget(String action, String paramTitle, String paramText, boolean isMethodPost,
                boolean isEncTypeMultipart, String[] fileNames, String[][] fileAccepts) {
            mData = new String[3];
            mData[ACTION_INDEX] = replaceNullWithEmpty(action);
            mData[PARAM_TITLE_INDEX] = replaceNullWithEmpty(paramTitle);
            mData[PARAM_TEXT_INDEX] = replaceNullWithEmpty(paramText);
            mIsShareMethodPost = isMethodPost;
            mIsShareEncTypeMultipart = isEncTypeMultipart;

            mFileNames = fileNames != null ? fileNames : new String[0];
            mFileAccepts = fileAccepts != null ? fileAccepts : new String[0][];
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof ShareTarget)) return false;
            ShareTarget shareTarget = (ShareTarget) o;
            return Arrays.equals(mData, shareTarget.mData)
                    && mIsShareMethodPost == shareTarget.mIsShareMethodPost
                    && mIsShareEncTypeMultipart == shareTarget.mIsShareEncTypeMultipart
                    && Arrays.equals(mFileNames, shareTarget.mFileNames)
                    && Arrays.deepEquals(mFileAccepts, shareTarget.mFileAccepts);
        }

        public String getAction() {
            return mData[ACTION_INDEX];
        }

        public String getParamTitle() {
            return mData[PARAM_TITLE_INDEX];
        }

        public String getParamText() {
            return mData[PARAM_TEXT_INDEX];
        }

        public boolean isShareMethodPost() {
            return mIsShareMethodPost;
        }

        public boolean isShareEncTypeMultipart() {
            return mIsShareEncTypeMultipart;
        }

        public String[] getFileNames() {
            return mFileNames;
        }

        public String[][] getFileAccepts() {
            return mFileAccepts;
        }
    }

    public static WebApkInfo createEmpty() {
        return create(WebApkIntentDataProvider.createEmpty());
    }

    /**
     * Constructs a WebApkInfo from the passed in Intent and <meta-data> in the WebAPK's Android
     * manifest.
     * @param intent Intent containing info about the app.
     */
    public static WebApkInfo create(Intent intent) {
        return create(WebApkIntentDataProvider.create(intent));
    }

    /**
     * Constructs a WebApkInfo from the passed in parameters and <meta-data> in the WebAPK's Android
     * manifest.
     *
     * @param webApkPackageName The package name of the WebAPK.
     * @param url Url that the WebAPK should navigate to when launched.
     * @param source Source that the WebAPK was launched from.
     * @param forceNavigation Whether the WebAPK should navigate to {@link url} if it is already
     *                        running.
     * @param canUseSplashFromContentProvider Whether the WebAPK's content provider can be
     *                                        queried for a screenshot of the splash screen.
     * @param shareData Shared information from the share intent.
     * @param shareDataActivityClassName Name of WebAPK activity which received share intent.
     */
    public static WebApkInfo create(String webApkPackageName, String url, int source,
            boolean forceNavigation, boolean canUseSplashFromContentProvider, ShareData shareData,
            String shareDataActivityClassName) {
        return create(
                WebApkIntentDataProvider.create(webApkPackageName, url, source, forceNavigation,
                        canUseSplashFromContentProvider, shareData, shareDataActivityClassName));
    }

    /**
     * Construct a {@link WebApkInfo} instance.
     * @param url                      URL that the WebAPK should navigate to when launched.
     * @param scope                    Scope for the WebAPK.
     * @param primaryIcon              Primary icon to show for the WebAPK.
     * @param badgeIcon                Badge icon to use for notifications.
     * @param splashIcon               Splash icon to use for the splash screen.
     * @param name                     Name of the WebAPK.
     * @param shortName                The short name of the WebAPK.
     * @param displayMode              Display mode of the WebAPK.
     * @param orientation              Orientation of the WebAPK.
     * @param source                   Source that the WebAPK was launched from.
     * @param themeColor               The theme color of the WebAPK.
     * @param backgroundColor          The background color of the WebAPK.
     * @param defaultBackgroundColor   The background color to use if the Web Manifest does not
     *                                 provide a background color.
     * @param isPrimaryIconMaskable    Is the primary icon maskable.
     * @param webApkPackageName        The package of the WebAPK.
     * @param shellApkVersion          Version of the code in //chrome/android/webapk/shell_apk.
     * @param manifestUrl              URL of the Web Manifest.
     * @param manifestStartUrl         URL that the WebAPK should navigate to when launched from
     *                                 the homescreen. Different from the {@link url} parameter if
     *                                 the WebAPK is launched from a deep link.
     * @param distributor              The source from where the WebAPK is installed.
     * @param iconUrlToMurmur2HashMap  Map of the WebAPK's icon URLs to Murmur2 hashes of the
     *                                 icon untransformed bytes.
     * @param shareTarget              Specifies what share data is supported by WebAPK.
     * @param forceNavigation          Whether the WebAPK should navigate to {@link url} if the
     *                                 WebAPK is already open.
     * @param isSplashProvidedByWebApk Whether the WebAPK (1) launches an internal activity to
     *                                 display the splash screen and (2) has a content provider
     *                                 which provides a screenshot of the splash screen.
     * @param shareData                Shared information from the share intent.
     * @param webApkVersionCode        WebAPK's version code.
     */
    public static WebApkInfo create(String url, String scope, WebappIcon primaryIcon,
            WebappIcon badgeIcon, WebappIcon splashIcon, String name, String shortName,
            @WebDisplayMode int displayMode, int orientation, int source, long themeColor,
            long backgroundColor, int defaultBackgroundColor, boolean isPrimaryIconMaskable,
            boolean isSplashIconMaskable, String webApkPackageName, int shellApkVersion,
            String manifestUrl, String manifestStartUrl, @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap, ShareTarget shareTarget,
            boolean forceNavigation, boolean isSplashProvidedByWebApk, ShareData shareData,
            int webApkVersionCode) {
        return create(WebApkIntentDataProvider.create(url, scope, primaryIcon, badgeIcon,
                splashIcon, name, shortName, displayMode, orientation, source, themeColor,
                backgroundColor, defaultBackgroundColor, isPrimaryIconMaskable,
                isSplashIconMaskable, webApkPackageName, shellApkVersion, manifestUrl,
                manifestStartUrl, distributor, iconUrlToMurmur2HashMap, shareTarget,
                forceNavigation, isSplashProvidedByWebApk, shareData, webApkVersionCode));
    }

    private static WebApkInfo create(@Nullable BrowserServicesIntentDataProvider provider) {
        return (provider != null) ? new WebApkInfo(provider) : null;
    }

    public WebApkInfo(@NonNull BrowserServicesIntentDataProvider provider) {
        super(provider);
    }

    /**
     * Returns the badge icon in Bitmap form.
     */
    public WebappIcon badgeIcon() {
        return getWebApkExtras().badgeIcon;
    }

    /**
     * Returns the splash icon in Bitmap form.
     */
    public WebappIcon splashIcon() {
        return getWebApkExtras().splashIcon;
    }

    public boolean isSplashIconMaskable() {
        return getWebApkExtras().isSplashIconMaskable;
    }

    /** Returns data about the WebAPK's share intent handlers. */
    public ShareTarget shareTarget() {
        return getWebApkExtras().shareTarget;
    }

    /**
     * Returns the WebAPK's version code.
     */
    public int webApkVersionCode() {
        return getWebApkExtras().webApkVersionCode;
    }

    @Override
    public boolean isForWebApk() {
        return true;
    }

    @Override
    public String webApkPackageName() {
        return getWebApkExtras().webApkPackageName;
    }

    @Override
    public boolean isSplashProvidedByWebApk() {
        return getWebApkExtras().isSplashProvidedByWebApk;
    }

    public int shellApkVersion() {
        return getWebApkExtras().shellApkVersion;
    }

    public String manifestUrl() {
        return getWebApkExtras().manifestUrl;
    }

    public String manifestStartUrl() {
        return getWebApkExtras().manifestStartUrl;
    }

    public @WebApkDistributor int distributor() {
        return getWebApkExtras().distributor;
    }

    public Map<String, String> iconUrlToMurmur2HashMap() {
        return getWebApkExtras().iconUrlToMurmur2HashMap;
    }

    public ShareData shareData() {
        return getWebApkExtras().shareData;
    }

    private WebApkExtras getWebApkExtras() {
        WebApkExtras extras = mProvider.getWebApkExtras();
        assert extras != null;
        return extras;
    }

    @Override
    public void setWebappIntentExtras(Intent intent) {
        // For launching a {@link WebApkActivity}.
        intent.putExtra(ShortcutHelper.EXTRA_ID, id());
        intent.putExtra(ShortcutHelper.EXTRA_URL, url());
        intent.putExtra(ShortcutHelper.EXTRA_SOURCE, source());
        intent.putExtra(ShortcutHelper.EXTRA_FORCE_NAVIGATION, shouldForceNavigation());
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName());
        intent.putExtra(
                WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, isSplashProvidedByWebApk());
    }

    /** Returns the value if it is non-null. Returns an empty string otherwise. */
    private static String replaceNullWithEmpty(String value) {
        return (value == null) ? "" : value;
    }
}
