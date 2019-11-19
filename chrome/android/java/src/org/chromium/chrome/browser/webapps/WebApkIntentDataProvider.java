// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.webapps.WebApkInfo.ShareData;
import org.chromium.chrome.browser.webapps.WebApkInfo.ShareTarget;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Stores info for WebAPK.
 */
public class WebApkIntentDataProvider extends BrowserServicesIntentDataProvider {
    public static final String RESOURCE_NAME = "name";
    public static final String RESOURCE_SHORT_NAME = "short_name";
    public static final String RESOURCE_STRING_TYPE = "string";

    private static final String TAG = "WebApkInfo";

    private int mToolbarColor;
    private WebappExtras mWebappExtras;
    private WebApkExtras mWebApkExtras;

    /**
     * Returns the WebAPK's shell launch timestamp associated with the passed-in intent, or -1.
     */
    public static long getWebApkShellLaunchTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
    }

    /**
     * Copies the WebAPKs's shell launch timestamp, if set, from {@link fromIntent} to
     * {@link toIntent}.
     */
    public static void copyWebApkShellLaunchTime(Intent fromIntent, Intent toIntent) {
        toIntent.putExtra(
                WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, getWebApkShellLaunchTime(fromIntent));
    }

    /**
     * Returns the timestamp when the WebAPK shell showed the splash screen. Returns -1 if the
     * WebAPK shell did not show the splash screen.
     */
    public static long getNewStyleWebApkSplashShownTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME, -1);
    }

    /**
     * Copies the timestamp, if set, that the WebAPK shell showed the splash screen from
     * {@link fromIntent} to {@link toIntent}.
     */
    public static void copyNewStyleWebApkSplashShownTime(Intent fromIntent, Intent toIntent) {
        toIntent.putExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME,
                getNewStyleWebApkSplashShownTime(fromIntent));
    }

    public static WebApkIntentDataProvider createEmpty() {
        return new WebApkIntentDataProvider(WebappIntentDataProvider.getDefaultToolbarColor(),
                WebappExtras.createEmpty(), WebApkExtras.createEmpty());
    }

    /**
     * Constructs a WebApkIntentDataProvider from the passed in Intent and <meta-data> in the
     * WebAPK's Android manifest.
     * @param intent Intent containing info about the app.
     */
    public static WebApkIntentDataProvider create(Intent intent) {
        String webApkPackageName =
                IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);

        if (TextUtils.isEmpty(webApkPackageName)) {
            return null;
        }

        // Force navigation if the extra is not specified to avoid breaking deep linking for old
        // WebAPKs which don't specify the {@link ShortcutHelper#EXTRA_FORCE_NAVIGATION} intent
        // extra.
        boolean forceNavigation = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_FORCE_NAVIGATION, true);

        ShareData shareData = null;

        String shareDataActivityClassName = IntentUtils.safeGetStringExtra(
                intent, WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);

        // Presence of {@link shareDataActivityClassName} indicates that this is a share.
        if (!TextUtils.isEmpty(shareDataActivityClassName)) {
            shareData = new ShareData();
            shareData.subject = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_SUBJECT);
            shareData.text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
            shareData.files = IntentUtils.getParcelableArrayListExtra(intent, Intent.EXTRA_STREAM);
            if (shareData.files == null) {
                Uri file = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_STREAM);
                if (file != null) {
                    shareData.files = new ArrayList<>();
                    shareData.files.add(file);
                }
            }
        }

        String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        int source = computeSource(intent, shareData);

        boolean canUseSplashFromContentProvider = IntentUtils.safeGetBooleanExtra(
                intent, WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, false);

        return create(webApkPackageName, url, source, forceNavigation,
                canUseSplashFromContentProvider, shareData, shareDataActivityClassName);
    }

    /**
     * Returns whether the WebAPK has a content provider which provides an image to use for the
     * splash screen.
     */
    private static boolean hasContentProviderForSplash(String webApkPackageName) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        ProviderInfo providerInfo = packageManager.resolveContentProvider(
                WebApkCommonUtils.generateSplashContentProviderAuthority(webApkPackageName), 0);
        return (providerInfo != null
                && TextUtils.equals(providerInfo.packageName, webApkPackageName));
    }

    private static @WebApkDistributor int getDistributor(Bundle bundle, String packageName) {
        String distributor = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.DISTRIBUTOR);
        if (!TextUtils.isEmpty(distributor)) {
            if (TextUtils.equals(distributor, "browser")) {
                return WebApkDistributor.BROWSER;
            }
            if (TextUtils.equals(distributor, "device_policy")) {
                return WebApkDistributor.DEVICE_POLICY;
            }
            return WebApkDistributor.OTHER;
        }
        return packageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX)
                ? WebApkDistributor.BROWSER
                : WebApkDistributor.OTHER;
    }

    /**
     * Constructs a WebApkIntentDataProvider from the passed in parameters and <meta-data> in the
     * WebAPK's Android manifest.
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
    public static WebApkIntentDataProvider create(String webApkPackageName, String url, int source,
            boolean forceNavigation, boolean canUseSplashFromContentProvider, ShareData shareData,
            String shareDataActivityClassName) {
        // Unlike non-WebAPK web apps, WebAPK ids are predictable. A malicious actor may send an
        // intent with a valid start URL and arbitrary other data. Only use the start URL, the
        // package name and the ShortcutSource from the launch intent and extract the remaining data
        // from the <meta-data> in the WebAPK's Android manifest.

        Bundle bundle = extractWebApkMetaData(webApkPackageName);
        if (bundle == null) {
            return null;
        }

        Context appContext = ContextUtils.getApplicationContext();
        PackageManager pm = appContext.getPackageManager();
        Resources res = null;
        int apkVersion = 0;
        try {
            res = pm.getResourcesForApplication(webApkPackageName);
            apkVersion = pm.getPackageInfo(webApkPackageName, 0).versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        int nameId = res.getIdentifier(RESOURCE_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        int shortNameId =
                res.getIdentifier(RESOURCE_SHORT_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        String name = nameId != 0 ? res.getString(nameId)
                                  : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.NAME);
        String shortName = shortNameId != 0
                ? res.getString(shortNameId)
                : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SHORT_NAME);

        String scope = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SCOPE);

        @WebDisplayMode
        int displayMode = displayModeFromString(
                IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.DISPLAY_MODE));
        int orientation = orientationFromString(
                IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.ORIENTATION));
        long themeColor = WebApkMetaDataUtils.getLongFromMetaData(bundle,
                WebApkMetaDataKeys.THEME_COLOR, ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        long backgroundColor =
                WebApkMetaDataUtils.getLongFromMetaData(bundle, WebApkMetaDataKeys.BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);

        // Fetch the default background color from the WebAPK's resources. Fetching the default
        // background color from the WebAPK is important for consistency when:
        // - A new version of Chrome has changed the default background color.
        // - Chrome has not yet requested an update for the WebAPK and the WebAPK still has the old
        //   default background color in its resources.
        // New-style WebAPKs use the background color and default background color in both the
        // WebAPK and Chrome processes.
        int defaultBackgroundColorId =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID, 0);
        int defaultBackgroundColor = (defaultBackgroundColorId == 0)
                ? SplashLayout.getDefaultBackgroundColor(appContext)
                : ApiCompatibilityUtils.getColor(res, defaultBackgroundColorId);

        int shellApkVersion =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SHELL_APK_VERSION, 0);

        String manifestUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.WEB_MANIFEST_URL);
        String manifestStartUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.START_URL);
        Map<String, String> iconUrlToMurmur2HashMap = getIconUrlAndIconMurmur2HashMap(bundle);

        @WebApkDistributor
        int distributor = getDistributor(bundle, webApkPackageName);

        int primaryIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.ICON_ID, 0);
        int primaryMaskableIconId =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.MASKABLE_ICON_ID, 0);

        // There are a few WebAPKs with bad shells (between v105 and v114) that would previously
        // cause chrome to crash. The check below fixes it. See crbug.com/1019318#c8 for details.
        if (shellApkVersion >= 105 && shellApkVersion <= 114) {
            try {
                ApiCompatibilityUtils.getDrawable(res, primaryMaskableIconId);
            } catch (Resources.NotFoundException e) {
                primaryMaskableIconId = 0;
            }
        }

        boolean isPrimaryIconMaskable =
                primaryMaskableIconId != 0 && ShortcutHelper.doesAndroidSupportMaskableIcons();

        int badgeIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.BADGE_ICON_ID, 0);
        int splashIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SPLASH_ID, 0);

        int isSplashIconMaskableBooleanId = IntentUtils.safeGetInt(
                bundle, WebApkMetaDataKeys.IS_SPLASH_ICON_MASKABLE_BOOLEAN_ID, 0);
        boolean isSplashIconMaskable = false;
        if (isSplashIconMaskableBooleanId != 0) {
            try {
                isSplashIconMaskable = res.getBoolean(isSplashIconMaskableBooleanId);
            } catch (Resources.NotFoundException e) {
            }
        }

        Pair<String, ShareTarget> shareTargetActivityNameAndData =
                extractFirstShareTarget(webApkPackageName);
        ShareTarget shareTarget = shareTargetActivityNameAndData.second;
        if (shareDataActivityClassName != null
                && !shareDataActivityClassName.equals(shareTargetActivityNameAndData.first)) {
            shareData = null;
        }

        boolean isSplashProvidedByWebApk =
                (canUseSplashFromContentProvider && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                        && hasContentProviderForSplash(webApkPackageName));

        return create(url, scope,
                new WebappIcon(webApkPackageName,
                        isPrimaryIconMaskable ? primaryMaskableIconId : primaryIconId),
                new WebappIcon(webApkPackageName, badgeIconId),
                new WebappIcon(webApkPackageName, splashIconId), name, shortName, displayMode,
                orientation, source, themeColor, backgroundColor, defaultBackgroundColor,
                isPrimaryIconMaskable, isSplashIconMaskable, webApkPackageName, shellApkVersion,
                manifestUrl, manifestStartUrl, distributor, iconUrlToMurmur2HashMap, shareTarget,
                forceNavigation, isSplashProvidedByWebApk, shareData, apkVersion);
    }

    /**
     * Construct a {@link WebApkIntentDataProvider} instance.
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
     * @param isSplashIconMaskable     Is the splash icon maskable.
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
    public static WebApkIntentDataProvider create(String url, String scope, WebappIcon primaryIcon,
            WebappIcon badgeIcon, WebappIcon splashIcon, String name, String shortName,
            @WebDisplayMode int displayMode, int orientation, int source, long themeColor,
            long backgroundColor, int defaultBackgroundColor, boolean isPrimaryIconMaskable,
            boolean isSplashIconMaskable, String webApkPackageName, int shellApkVersion,
            String manifestUrl, String manifestStartUrl, @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap, ShareTarget shareTarget,
            boolean forceNavigation, boolean isSplashProvidedByWebApk, ShareData shareData,
            int webApkVersionCode) {
        if (manifestStartUrl == null || webApkPackageName == null) {
            Log.e(TAG, "Incomplete data provided: " + manifestStartUrl + ", " + webApkPackageName);
            return null;
        }

        if (TextUtils.isEmpty(url)) {
            url = manifestStartUrl;
        }

        // The default scope should be computed from the Web Manifest start URL. If the WebAPK was
        // launched from a deep link {@link startUrl} may be different from the Web Manifest start
        // URL.
        if (TextUtils.isEmpty(scope)) {
            scope = ShortcutHelper.getScopeFromUrl(manifestStartUrl);
        }

        if (primaryIcon == null) {
            primaryIcon = new WebappIcon();
        }

        if (badgeIcon == null) {
            badgeIcon = new WebappIcon();
        }

        if (splashIcon == null) {
            splashIcon = new WebappIcon();
        }

        if (shareTarget == null) {
            shareTarget = new ShareTarget();
        }

        WebappExtras webappExtras =
                new WebappExtras(WebappRegistry.webApkIdForPackage(webApkPackageName), url, scope,
                        primaryIcon, name, shortName, displayMode, orientation, source,
                        WebappIntentDataProvider.isLongColorValid(themeColor),
                        WebappIntentDataProvider.colorFromLongColor(backgroundColor),
                        defaultBackgroundColor, false /* isIconGenerated */, isPrimaryIconMaskable,
                        forceNavigation);
        WebApkExtras webApkExtras = new WebApkExtras(webApkPackageName, badgeIcon, splashIcon,
                isSplashIconMaskable, shellApkVersion, manifestUrl, manifestStartUrl, distributor,
                iconUrlToMurmur2HashMap, shareTarget, isSplashProvidedByWebApk, shareData,
                webApkVersionCode);
        int toolbarColor = webappExtras.hasCustomToolbarColor
                ? (int) themeColor
                : WebappIntentDataProvider.getDefaultToolbarColor();
        return new WebApkIntentDataProvider(toolbarColor, webappExtras, webApkExtras);
    }

    private WebApkIntentDataProvider(
            int toolbarColor, WebappExtras webappExtras, WebApkExtras webApkExtras) {
        mToolbarColor = toolbarColor;
        mWebappExtras = webappExtras;
        mWebApkExtras = webApkExtras;
    }

    private static int computeSource(Intent intent, ShareData shareData) {
        int source = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        if (source >= ShortcutSource.COUNT) {
            return ShortcutSource.UNKNOWN;
        }
        if (source == ShortcutSource.EXTERNAL_INTENT
                && IntentHandler.determineExternalIntentSource(intent)
                        == IntentHandler.ExternalAppId.CHROME) {
            return ShortcutSource.EXTERNAL_INTENT_FROM_CHROME;
        }

        if (source == ShortcutSource.WEBAPK_SHARE_TARGET && shareData != null
                && shareData.files != null && shareData.files.size() > 0) {
            return ShortcutSource.WEBAPK_SHARE_TARGET_FILE;
        }
        return source;
    }

    /**
     * Extracts meta data from a WebAPK's Android Manifest.
     * @param webApkPackageName WebAPK's package name.
     * @return Bundle with the extracted meta data.
     */
    private static Bundle extractWebApkMetaData(String webApkPackageName) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        try {
            ApplicationInfo appInfo = packageManager.getApplicationInfo(
                    webApkPackageName, PackageManager.GET_META_DATA);
            return appInfo.metaData;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Extract the icon URLs and icon hashes from the WebAPK's meta data, and returns a map of these
     * {URL, hash} pairs. The icon URLs/icon hashes are stored in a single meta data tag in the
     * WebAPK's AndroidManifest.xml as following:
     * "URL1 hash1 URL2 hash2 URL3 hash3..."
     */
    private static Map<String, String> getIconUrlAndIconMurmur2HashMap(Bundle metaData) {
        Map<String, String> iconUrlAndIconMurmur2HashMap = new HashMap<String, String>();
        String iconUrlsAndIconMurmur2Hashes =
                metaData.getString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES);
        if (TextUtils.isEmpty(iconUrlsAndIconMurmur2Hashes)) return iconUrlAndIconMurmur2HashMap;

        // Parse the metadata tag which contains "URL1 hash1 URL2 hash2 URL3 hash3..." pairs and
        // create a hash map.
        // TODO(hanxi): crbug.com/666349. Add a test to verify that the icon URLs in WebAPKs'
        // AndroidManifest.xml don't contain space.
        String[] urlsAndHashes = iconUrlsAndIconMurmur2Hashes.split("[ ]+");
        if (urlsAndHashes.length % 2 != 0) {
            Log.e(TAG, "The icon URLs and icon murmur2 hashes don't come in pairs.");
            return iconUrlAndIconMurmur2HashMap;
        }
        for (int i = 0; i < urlsAndHashes.length; i += 2) {
            iconUrlAndIconMurmur2HashMap.put(urlsAndHashes[i], urlsAndHashes[i + 1]);
        }
        return iconUrlAndIconMurmur2HashMap;
    }

    /**
     * Returns the WebDisplayMode which matches {@link displayMode}.
     * @param displayMode One of https://www.w3.org/TR/appmanifest/#dfn-display-modes-values
     * @return The matching WebDisplayMode. {@link WebDisplayMode#Undefined} if there is no match.
     */
    private static @WebDisplayMode int displayModeFromString(String displayMode) {
        if (displayMode == null) {
            return WebDisplayMode.UNDEFINED;
        }

        if (displayMode.equals("fullscreen")) {
            return WebDisplayMode.FULLSCREEN;
        } else if (displayMode.equals("standalone")) {
            return WebDisplayMode.STANDALONE;
        } else if (displayMode.equals("minimal-ui")) {
            return WebDisplayMode.MINIMAL_UI;
        } else if (displayMode.equals("browser")) {
            return WebDisplayMode.BROWSER;
        } else {
            return WebDisplayMode.UNDEFINED;
        }
    }

    /**
     * Returns the ScreenOrientationValue which matches {@link orientation}.
     * @param orientation One of https://w3c.github.io/screen-orientation/#orientationlocktype-enum
     * @return The matching ScreenOrientationValue. {@link ScreenOrientationValues#DEFAULT} if there
     * is no match.
     */
    private static int orientationFromString(String orientation) {
        if (orientation == null) {
            return ScreenOrientationValues.DEFAULT;
        }

        if (orientation.equals("any")) {
            return ScreenOrientationValues.ANY;
        } else if (orientation.equals("natural")) {
            return ScreenOrientationValues.NATURAL;
        } else if (orientation.equals("landscape")) {
            return ScreenOrientationValues.LANDSCAPE;
        } else if (orientation.equals("landscape-primary")) {
            return ScreenOrientationValues.LANDSCAPE_PRIMARY;
        } else if (orientation.equals("landscape-secondary")) {
            return ScreenOrientationValues.LANDSCAPE_SECONDARY;
        } else if (orientation.equals("portrait")) {
            return ScreenOrientationValues.PORTRAIT;
        } else if (orientation.equals("portrait-primary")) {
            return ScreenOrientationValues.PORTRAIT_PRIMARY;
        } else if (orientation.equals("portrait-secondary")) {
            return ScreenOrientationValues.PORTRAIT_SECONDARY;
        } else {
            return ScreenOrientationValues.DEFAULT;
        }
    }

    /**
     * Returns the name of activity or activity alias in WebAPK which handles share intents, and
     * the data about the handler.
     */
    private static Pair<String, ShareTarget> extractFirstShareTarget(String webApkPackageName) {
        Intent shareIntent = new Intent();
        shareIntent.setAction(Intent.ACTION_SEND);
        shareIntent.setPackage(webApkPackageName);
        shareIntent.setType("*/*");
        List<ResolveInfo> resolveInfos = PackageManagerUtils.queryIntentActivities(
                shareIntent, PackageManager.GET_META_DATA);

        for (ResolveInfo resolveInfo : resolveInfos) {
            Bundle shareTargetMetaData = resolveInfo.activityInfo.metaData;
            if (shareTargetMetaData == null) {
                continue;
            }

            String shareTargetActivityName = resolveInfo.activityInfo.name;

            String shareAction =
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_ACTION);
            if (TextUtils.isEmpty(shareAction)) {
                return new Pair<>(null, new ShareTarget());
            }

            String encodedFileNames = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_NAMES);
            String[] fileNames = WebApkShareTargetUtil.decodeJsonStringArray(encodedFileNames);

            String encodedFileAccepts = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS);
            String[][] fileAccepts = WebApkShareTargetUtil.decodeJsonAccepts(encodedFileAccepts);

            String shareMethod =
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_METHOD);
            boolean isShareMethodPost =
                    shareMethod != null && shareMethod.toUpperCase(Locale.ENGLISH).equals("POST");

            String shareEncType = IntentUtils.safeGetString(
                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_ENCTYPE);
            boolean isShareEncTypeMultipart = shareEncType != null
                    && shareEncType.toLowerCase(Locale.ENGLISH).equals("multipart/form-data");

            ShareTarget target = new ShareTarget(
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_ACTION),
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TITLE),
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                    isShareMethodPost, isShareEncTypeMultipart, fileNames, fileAccepts);

            return new Pair<>(shareTargetActivityName, target);
        }
        return new Pair<>(null, new ShareTarget());
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public int getTitleVisibilityState() {
        return CustomTabsIntent.SHOW_PAGE_TITLE;
    }

    @Override
    @Nullable
    public WebappExtras getWebappExtras() {
        return mWebappExtras;
    }

    @Override
    @Nullable
    public WebApkExtras getWebApkExtras() {
        return mWebApkExtras;
    }
}
