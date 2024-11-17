// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.components.webapk.lib.common.WebApkConstants.WEBAPK_PACKAGE_PREFIX;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK;
import static org.chromium.webapk.lib.common.WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.content.res.XmlResourceParser;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.trusted.sharing.ShareData;

import org.xmlpull.v1.XmlPullParser;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.WebApkMetaDataUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/** Factory for building {@link BrowserServicesIntentDataProvider} for WebAPKs. */
public class WebApkIntentDataProviderFactory {
    public static final String RESOURCE_NAME = "name";
    public static final String RESOURCE_SHORT_NAME = "short_name";
    public static final String RESOURCE_SHORTCUTS = "shortcuts";
    public static final String RESOURCE_STRING_TYPE = "string";
    public static final String RESOURCE_XML_TYPE = "xml";

    private static final String SHORTCUT_ATTRIBUTE_NAMESPACE =
            "http://schemas.android.com/apk/res/android";
    private static final String SHORTCUT_TAG_NAME = "shortcut";
    private static final String SHORTCUT_INTENT_TAG_NAME = "intent";
    private static final String SHORTCUT_NAME_ATTRIBUTE = "shortcutLongLabel";
    private static final String SHORTCUT_SHORT_NAME_ATTRIBUTE = "shortcutShortLabel";
    private static final String SHORTCUT_ICON_HASH_ATTRIBUTE = "iconHash";
    private static final String SHORTCUT_ICON_URL_ATTRIBUTE = "iconUrl";
    private static final String SHORTCUT_ICON_ATTRIBUTE = "icon";
    private static final String SHORTCUT_INTENT_LAUNCH_URL_ATTRIBUTE = "data";

    private static final String TAG = "WebApkInfo";

    /**
     * Constructs a BrowserServicesIntentDataProvider from the passed in Intent and <meta-data> in
     * the WebAPK's Android manifest.
     * @param intent Intent containing info about the app.
     */
    public static BrowserServicesIntentDataProvider create(Intent intent) {
        String webApkPackageName = WebappIntentUtils.getWebApkPackageName(intent);

        if (TextUtils.isEmpty(webApkPackageName)) {
            return null;
        }

        // Force navigation if the extra is not specified to avoid breaking deep linking for old
        // WebAPKs which don't specify the {@link WebappConstants#EXTRA_FORCE_NAVIGATION} intent
        // extra.
        boolean forceNavigation =
                IntentUtils.safeGetBooleanExtra(
                        intent, WebappConstants.EXTRA_FORCE_NAVIGATION, true);

        ShareData shareData = null;

        String shareDataActivityClassName =
                IntentUtils.safeGetStringExtra(
                        intent, EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME);

        // Presence of {@link shareDataActivityClassName} indicates that this is a share.
        if (!TextUtils.isEmpty(shareDataActivityClassName)) {
            String subject = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_SUBJECT);
            String text = IntentUtils.safeGetStringExtra(intent, Intent.EXTRA_TEXT);
            List<Uri> files = IntentUtils.getParcelableArrayListExtra(intent, Intent.EXTRA_STREAM);
            if (files == null) {
                Uri file = IntentUtils.safeGetParcelableExtra(intent, Intent.EXTRA_STREAM);
                if (file != null) {
                    files = new ArrayList<>();
                    files.add(file);
                }
            }
            shareData = new ShareData(subject, text, files);
        }

        String url = WebappIntentUtils.getUrl(intent);
        int source = computeSource(intent, shareData);

        boolean canUseSplashFromContentProvider =
                IntentUtils.safeGetBooleanExtra(intent, EXTRA_SPLASH_PROVIDED_BY_WEBAPK, false);

        return create(
                intent,
                webApkPackageName,
                url,
                source,
                forceNavigation,
                canUseSplashFromContentProvider,
                shareData,
                shareDataActivityClassName);
    }

    /**
     * Returns whether the WebAPK has a content provider which provides an image to use for the
     * splash screen.
     */
    private static boolean hasContentProviderForSplash(String webApkPackageName) {
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        ProviderInfo providerInfo =
                packageManager.resolveContentProvider(
                        WebApkCommonUtils.generateSplashContentProviderAuthority(webApkPackageName),
                        0);
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
        return packageName.startsWith(WEBAPK_PACKAGE_PREFIX)
                ? WebApkDistributor.BROWSER
                : WebApkDistributor.OTHER;
    }

    /**
     * @return A list of shortcut items derived from the parser.
     */
    // looking up resources from other apps requires the use of getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    private static List<ShortcutItem> parseShortcutItems(String webApkPackageName, Resources res) {
        int shortcutsResId =
                res.getIdentifier(RESOURCE_SHORTCUTS, RESOURCE_XML_TYPE, webApkPackageName);
        if (shortcutsResId == 0) {
            return new ArrayList<>();
        }

        XmlResourceParser parser = res.getXml(shortcutsResId);
        List<ShortcutItem> shortcuts = new ArrayList<>();
        try {
            int eventType = parser.getEventType();
            while (eventType != XmlPullParser.END_DOCUMENT) {
                if (eventType == XmlPullParser.START_TAG
                        && TextUtils.equals(parser.getName(), SHORTCUT_TAG_NAME)) {
                    int nameResId =
                            parser.getAttributeResourceValue(
                                    SHORTCUT_ATTRIBUTE_NAMESPACE, SHORTCUT_NAME_ATTRIBUTE, 0);
                    int shortNameResId =
                            parser.getAttributeResourceValue(
                                    SHORTCUT_ATTRIBUTE_NAMESPACE, SHORTCUT_SHORT_NAME_ATTRIBUTE, 0);
                    String iconUrl = parser.getAttributeValue(null, SHORTCUT_ICON_URL_ATTRIBUTE);
                    String iconHash = parser.getAttributeValue(null, SHORTCUT_ICON_HASH_ATTRIBUTE);
                    int iconId =
                            parser.getAttributeResourceValue(
                                    SHORTCUT_ATTRIBUTE_NAMESPACE, SHORTCUT_ICON_ATTRIBUTE, 0);

                    eventType = parser.next();
                    if (eventType != XmlPullParser.START_TAG
                            && !TextUtils.equals(parser.getName(), SHORTCUT_INTENT_TAG_NAME)) {
                        // shortcuts.xml is malformed for some reason. Bail out.
                        return new ArrayList<>();
                    }

                    String launchUrl =
                            parser.getAttributeValue(
                                    SHORTCUT_ATTRIBUTE_NAMESPACE,
                                    SHORTCUT_INTENT_LAUNCH_URL_ATTRIBUTE);

                    shortcuts.add(
                            new ShortcutItem(
                                    nameResId != 0 ? res.getString(nameResId) : "",
                                    shortNameResId != 0 ? res.getString(shortNameResId) : "",
                                    launchUrl,
                                    iconUrl,
                                    iconHash,
                                    new WebappIcon(webApkPackageName, iconId)));
                }
                eventType = parser.next();
            }
        } catch (Exception e) {
            return new ArrayList<>();
        }

        return shortcuts;
    }

    /**
     * Constructs a BrowserServicesIntentDataProvider from the passed in parameters and <meta-data>
     * in the WebAPK's Android manifest.
     *
     * @param intent Intent used to launch activity.
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
    // looking up resources from other apps requires the use of getIdentifier()
    @SuppressWarnings("DiscouragedApi")
    public static BrowserServicesIntentDataProvider create(
            Intent intent,
            String webApkPackageName,
            String url,
            int source,
            boolean forceNavigation,
            boolean canUseSplashFromContentProvider,
            ShareData shareData,
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
        long lastUpdateTime = 0;
        try {
            res = pm.getResourcesForApplication(webApkPackageName);
            PackageInfo packageInfo = pm.getPackageInfo(webApkPackageName, 0);
            apkVersion = packageInfo.versionCode;
            lastUpdateTime = packageInfo.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        // Difficult to necessarily get the right theme from the other app. Use null instead.
        Theme theme = null;

        int nameId = res.getIdentifier(RESOURCE_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        int shortNameId =
                res.getIdentifier(RESOURCE_SHORT_NAME, RESOURCE_STRING_TYPE, webApkPackageName);
        String name =
                nameId != 0
                        ? res.getString(nameId)
                        : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.NAME);
        String shortName =
                shortNameId != 0
                        ? res.getString(shortNameId)
                        : IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SHORT_NAME);
        boolean hasCustomName = bundle.getBoolean(WebApkMetaDataKeys.HAS_CUSTOM_NAME, false);

        String scope = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.SCOPE);

        @DisplayMode.EnumType
        int displayMode =
                displayModeFromString(
                        IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.DISPLAY_MODE));
        int orientation =
                orientationFromString(
                        IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.ORIENTATION));
        long themeColor =
                WebApkMetaDataUtils.getLongFromMetaData(
                        bundle, WebApkMetaDataKeys.THEME_COLOR, ColorUtils.INVALID_COLOR);
        long backgroundColor =
                WebApkMetaDataUtils.getLongFromMetaData(
                        bundle, WebApkMetaDataKeys.BACKGROUND_COLOR, ColorUtils.INVALID_COLOR);
        long darkThemeColor =
                WebApkMetaDataUtils.getLongFromMetaData(
                        bundle, WebApkMetaDataKeys.DARK_THEME_COLOR, ColorUtils.INVALID_COLOR);
        long darkBackgroundColor =
                WebApkMetaDataUtils.getLongFromMetaData(
                        bundle, WebApkMetaDataKeys.DARK_BACKGROUND_COLOR, ColorUtils.INVALID_COLOR);

        // Fetch the default background color from the WebAPK's resources. Fetching the default
        // background color from the WebAPK is important for consistency when:
        // - A new version of Chrome has changed the default background color.
        // - Chrome has not yet requested an update for the WebAPK and the WebAPK still has the old
        //   default background color in its resources.
        // New-style WebAPKs use the background color and default background color in both the
        // WebAPK and Chrome processes.
        int defaultBackgroundColorId =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.DEFAULT_BACKGROUND_COLOR_ID, 0);
        int defaultBackgroundColor =
                (defaultBackgroundColorId == 0)
                        ? SplashLayout.getDefaultBackgroundColor(appContext)
                        : res.getColor(defaultBackgroundColorId, theme);

        int shellApkVersion =
                IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SHELL_APK_VERSION, 0);

        String manifestUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.WEB_MANIFEST_URL);
        String manifestStartUrl = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.START_URL);
        String manifestId = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.WEB_MANIFEST_ID);
        String appKey = IntentUtils.safeGetString(bundle, WebApkMetaDataKeys.APP_KEY);
        Map<String, String> iconUrlToMurmur2HashMap = getIconUrlAndIconMurmur2HashMap(bundle);

        @WebApkDistributor int distributor = getDistributor(bundle, webApkPackageName);

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

        // Check the OS version because the same WebAPK is vended by the WebAPK server for all OS
        // versions.
        boolean isPrimaryIconMaskable =
                primaryMaskableIconId != 0 && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O);

        int splashIconId = IntentUtils.safeGetInt(bundle, WebApkMetaDataKeys.SPLASH_ID, 0);

        int isSplashIconMaskableBooleanId =
                IntentUtils.safeGetInt(
                        bundle, WebApkMetaDataKeys.IS_SPLASH_ICON_MASKABLE_BOOLEAN_ID, 0);
        boolean isSplashIconMaskable = false;
        if (isSplashIconMaskableBooleanId != 0) {
            try {
                isSplashIconMaskable = res.getBoolean(isSplashIconMaskableBooleanId);
            } catch (Resources.NotFoundException e) {
            }
        }

        Pair<String, WebApkShareTarget> shareTargetActivityNameAndData =
                extractFirstShareTarget(webApkPackageName);
        WebApkShareTarget shareTarget = shareTargetActivityNameAndData.second;
        if (shareDataActivityClassName != null
                && !shareDataActivityClassName.equals(shareTargetActivityNameAndData.first)) {
            shareData = null;
        }

        boolean isSplashProvidedByWebApk =
                (canUseSplashFromContentProvider && hasContentProviderForSplash(webApkPackageName));

        return create(
                intent,
                url,
                scope,
                new WebappIcon(
                        webApkPackageName,
                        isPrimaryIconMaskable ? primaryMaskableIconId : primaryIconId,
                        res,
                        shellApkVersion),
                new WebappIcon(webApkPackageName, splashIconId),
                name,
                shortName,
                hasCustomName,
                displayMode,
                orientation,
                source,
                themeColor,
                backgroundColor,
                darkThemeColor,
                darkBackgroundColor,
                defaultBackgroundColor,
                isPrimaryIconMaskable,
                isSplashIconMaskable,
                webApkPackageName,
                shellApkVersion,
                manifestUrl,
                manifestStartUrl,
                manifestId,
                appKey,
                distributor,
                iconUrlToMurmur2HashMap,
                shareTarget,
                forceNavigation,
                isSplashProvidedByWebApk,
                shareData,
                parseShortcutItems(webApkPackageName, res),
                apkVersion,
                lastUpdateTime);
    }

    /**
     * Construct a {@link BrowserServicesIntentDataProvider} instance.
     *
     * @param intent Intent used to launch activity.
     * @param url URL that the WebAPK should navigate to when launched.
     * @param scope Scope for the WebAPK.
     * @param primaryIcon Primary icon to show for the WebAPK.
     * @param splashIcon Splash icon to use for the splash screen.
     * @param name Name of the WebAPK.
     * @param shortName The short name of the WebAPK.
     * @param displayMode Display mode of the WebAPK.
     * @param orientation Orientation of the WebAPK.
     * @param source Source that the WebAPK was launched from.
     * @param themeColor The theme color of the WebAPK.
     * @param backgroundColor The background color of the WebAPK.
     * @param darkThemeColor The theme color of the WebAPK's dark mode.
     * @param darkBackgroundColor The background color of the WebAPK's dark mode.
     * @param defaultBackgroundColor The background color to use if the Web Manifest does not
     *     provide a background color.
     * @param isPrimaryIconMaskable Is the primary icon maskable.
     * @param isSplashIconMaskable Is the splash icon maskable.
     * @param webApkPackageName The package of the WebAPK.
     * @param shellApkVersion Version of the code in //chrome/android/webapk/shell_apk.
     * @param manifestUrl URL of the Web Manifest.
     * @param manifestStartUrl URL that the WebAPK should navigate to when launched from the
     *     homescreen. Different from the {@link url} parameter if the WebAPK is launched from a
     *     deep link.
     * @param manifestId Id of the WebAPK.
     * @param appKey Key used to identified the WebAPK. This is either the Manifest URL or the
     *     Manifest Unique ID depending on the situation.
     * @param distributor The source from where the WebAPK is installed.
     * @param iconUrlToMurmur2HashMap Map of the WebAPK's icon URLs to Murmur2 hashes of the icon
     *     untransformed bytes.
     * @param shareTarget Specifies what share data is supported by WebAPK.
     * @param forceNavigation Whether the WebAPK should navigate to {@link url} if the WebAPK is
     *     already open.
     * @param isSplashProvidedByWebApk Whether the WebAPK (1) launches an internal activity to
     *     display the splash screen and (2) has a content provider which provides a screenshot of
     *     the splash screen.
     * @param shareData Shared information from the share intent.
     * @param shortcutItems A list of shortcut items.
     * @param webApkVersionCode WebAPK's version code.
     * @param lastUpdateTime WebAPK's last update timestamp.
     */
    public static BrowserServicesIntentDataProvider create(
            Intent intent,
            String url,
            String scope,
            WebappIcon primaryIcon,
            WebappIcon splashIcon,
            String name,
            String shortName,
            boolean hasCustomName,
            @DisplayMode.EnumType int displayMode,
            int orientation,
            int source,
            long themeColor,
            long backgroundColor,
            long darkThemeColor,
            long darkBackgroundColor,
            int defaultBackgroundColor,
            boolean isPrimaryIconMaskable,
            boolean isSplashIconMaskable,
            String webApkPackageName,
            int shellApkVersion,
            String manifestUrl,
            String manifestStartUrl,
            String manifestId,
            String appKey,
            @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap,
            WebApkShareTarget shareTarget,
            boolean forceNavigation,
            boolean isSplashProvidedByWebApk,
            ShareData shareData,
            List<ShortcutItem> shortcutItems,
            int webApkVersionCode,
            long lastUpdateTime) {
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

        if (TextUtils.isEmpty(appKey)) {
            appKey = manifestUrl;
        }

        if (primaryIcon == null) {
            primaryIcon = new WebappIcon();
        }

        if (splashIcon == null) {
            splashIcon = new WebappIcon();
        }

        WebappExtras webappExtras =
                new WebappExtras(
                        WebappIntentUtils.getIdForWebApkPackage(webApkPackageName),
                        url,
                        scope,
                        primaryIcon,
                        name,
                        shortName,
                        displayMode,
                        orientation,
                        source,
                        WebappIntentUtils.colorFromLongColor(backgroundColor),
                        WebappIntentUtils.colorFromLongColor(darkBackgroundColor),
                        defaultBackgroundColor,
                        /* isIconGenerated= */ false,
                        isPrimaryIconMaskable,
                        forceNavigation);
        WebApkExtras webApkExtras =
                new WebApkExtras(
                        webApkPackageName,
                        splashIcon,
                        isSplashIconMaskable,
                        shellApkVersion,
                        manifestUrl,
                        manifestStartUrl,
                        manifestId,
                        appKey,
                        distributor,
                        iconUrlToMurmur2HashMap,
                        shareTarget,
                        isSplashProvidedByWebApk,
                        shortcutItems,
                        webApkVersionCode,
                        lastUpdateTime,
                        hasCustomName);
        boolean hasCustomToolbarColor = WebappIntentUtils.isLongColorValid(themeColor);
        int toolbarColor =
                hasCustomToolbarColor
                        ? (int) themeColor
                        : WebappIntentDataProvider.getDefaultToolbarColor();
        boolean hasCustomDarkToolbarColor = WebappIntentUtils.isLongColorValid(darkThemeColor);
        int darkToolbarColor =
                hasCustomDarkToolbarColor
                        ? (int) darkThemeColor
                        : WebappIntentDataProvider.getDefaultDarkToolbarColor();
        return new WebappIntentDataProvider(
                intent,
                toolbarColor,
                hasCustomToolbarColor,
                darkToolbarColor,
                hasCustomDarkToolbarColor,
                shareData,
                webappExtras,
                webApkExtras);
    }

    private static int computeSource(Intent intent, ShareData shareData) {
        int source =
                IntentUtils.safeGetIntExtra(
                        intent, WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        if (source >= ShortcutSource.COUNT) {
            return ShortcutSource.UNKNOWN;
        }
        if (source == ShortcutSource.EXTERNAL_INTENT
                && IntentHandler.isExternalIntentSourceChrome(intent)) {
            return ShortcutSource.EXTERNAL_INTENT_FROM_CHROME;
        }

        if (source == ShortcutSource.WEBAPK_SHARE_TARGET
                && shareData != null
                && shareData.uris != null
                && shareData.uris.size() > 0) {
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
            ApplicationInfo appInfo =
                    packageManager.getApplicationInfo(
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
    @VisibleForTesting
    static Map<String, String> getIconUrlAndIconMurmur2HashMap(Bundle metaData) {
        Map<String, String> iconUrlAndIconMurmur2HashMap = new HashMap<String, String>();
        String iconUrlsAndIconMurmur2Hashes =
                metaData.getString(WebApkMetaDataKeys.ICON_URLS_AND_ICON_MURMUR2_HASHES);
        if (TextUtils.isEmpty(iconUrlsAndIconMurmur2Hashes)) return iconUrlAndIconMurmur2HashMap;

        // Parse the metadata tag which contains "URL1 hash1 URL2 hash2 URL3 hash3..." pairs and
        // create a hash map.
        String[] urlsAndHashes = iconUrlsAndIconMurmur2Hashes.split(" ");
        if (urlsAndHashes.length % 2 != 0) {
            Log.e(TAG, "The icon URLs and icon murmur2 hashes don't come in pairs.");
            return iconUrlAndIconMurmur2HashMap;
        }
        for (int i = 0; i < urlsAndHashes.length; i += 2) {
            if (!TextUtils.isEmpty(urlsAndHashes[i])) {
                iconUrlAndIconMurmur2HashMap.put(urlsAndHashes[i], urlsAndHashes[i + 1]);
            }
        }
        return iconUrlAndIconMurmur2HashMap;
    }

    /**
     * Returns the DisplayMode which matches {@link DisplayMode}.
     * @param displayMode One of https://www.w3.org/TR/appmanifest/#dfn-display-modes-values
     * @return The matching DisplayMode. {@link DisplayMode#Undefined} if there is no match.
     */
    private static @DisplayMode.EnumType int displayModeFromString(String displayMode) {
        if (displayMode == null) {
            return DisplayMode.UNDEFINED;
        }

        if (displayMode.equals("fullscreen")) {
            return DisplayMode.FULLSCREEN;
        } else if (displayMode.equals("standalone")) {
            return DisplayMode.STANDALONE;
        } else if (displayMode.equals("minimal-ui")) {
            return DisplayMode.MINIMAL_UI;
        } else if (displayMode.equals("browser")) {
            return DisplayMode.BROWSER;
        } else {
            return DisplayMode.UNDEFINED;
        }
    }

    /**
     * Returns the ScreenOrientationLockType which matches {@link orientation}.
     * @param orientation One of https://w3c.github.io/screen-orientation/#orientationlocktype-enum
     * @return The matching ScreenOrientationLockType. {@link ScreenOrientationLockType#DEFAULT} if
     *         there
     * is no match.
     */
    private static int orientationFromString(String orientation) {
        if (orientation == null) {
            return ScreenOrientationLockType.DEFAULT;
        }

        if (orientation.equals("any")) {
            return ScreenOrientationLockType.ANY;
        } else if (orientation.equals("natural")) {
            return ScreenOrientationLockType.NATURAL;
        } else if (orientation.equals("landscape")) {
            return ScreenOrientationLockType.LANDSCAPE;
        } else if (orientation.equals("landscape-primary")) {
            return ScreenOrientationLockType.LANDSCAPE_PRIMARY;
        } else if (orientation.equals("landscape-secondary")) {
            return ScreenOrientationLockType.LANDSCAPE_SECONDARY;
        } else if (orientation.equals("portrait")) {
            return ScreenOrientationLockType.PORTRAIT;
        } else if (orientation.equals("portrait-primary")) {
            return ScreenOrientationLockType.PORTRAIT_PRIMARY;
        } else if (orientation.equals("portrait-secondary")) {
            return ScreenOrientationLockType.PORTRAIT_SECONDARY;
        } else {
            return ScreenOrientationLockType.DEFAULT;
        }
    }

    /**
     * Returns the name of activity or activity alias in WebAPK which handles share intents, and
     * the data about the handler.
     */
    @NonNull
    private static Pair<String, WebApkShareTarget> extractFirstShareTarget(
            String webApkPackageName) {
        Intent shareIntent = new Intent();
        shareIntent.setAction(Intent.ACTION_SEND);
        shareIntent.setPackage(webApkPackageName);
        shareIntent.setType("*/*");
        List<ResolveInfo> resolveInfos =
                PackageManagerUtils.queryIntentActivities(
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
                return new Pair<>(null, null);
            }

            String encodedFileNames =
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_NAMES);
            String[] fileNames = WebApkShareTargetUtil.decodeJsonStringArray(encodedFileNames);

            String encodedFileAccepts =
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_ACCEPTS);
            String[][] fileAccepts = WebApkShareTargetUtil.decodeJsonAccepts(encodedFileAccepts);

            String shareMethod =
                    IntentUtils.safeGetString(shareTargetMetaData, WebApkMetaDataKeys.SHARE_METHOD);
            boolean isShareMethodPost =
                    shareMethod != null && shareMethod.toUpperCase(Locale.ENGLISH).equals("POST");

            String shareEncType =
                    IntentUtils.safeGetString(
                            shareTargetMetaData, WebApkMetaDataKeys.SHARE_ENCTYPE);
            boolean isShareEncTypeMultipart =
                    shareEncType != null
                            && shareEncType
                                    .toLowerCase(Locale.ENGLISH)
                                    .equals("multipart/form-data");

            WebApkShareTarget target =
                    new WebApkShareTarget(
                            shareAction,
                            IntentUtils.safeGetString(
                                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TITLE),
                            IntentUtils.safeGetString(
                                    shareTargetMetaData, WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                            isShareMethodPost,
                            isShareEncTypeMultipart,
                            fileNames,
                            fileAccepts);

            return new Pair<>(shareTargetActivityName, target);
        }
        return new Pair<>(null, null);
    }
}
