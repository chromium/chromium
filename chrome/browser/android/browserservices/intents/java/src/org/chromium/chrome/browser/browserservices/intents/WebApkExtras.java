// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.webapps.WebApkDistributor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Stores WebAPK specific information on behalf of {@link BrowserServicesIntentDataProvider}. */
public class WebApkExtras {
    /** The package of the WebAPK. */
    public final String webApkPackageName;

    /** Icon to use for the splash screen. */
    @NonNull public final WebappIcon splashIcon;

    /** Whether the WebAPK's splash icon should be masked. */
    public final boolean isSplashIconMaskable;

    /** Version of the code in //chrome/android/webapk/shell_apk. */
    public final int shellApkVersion;

    /** URL of the Web Manifest. */
    public final String manifestUrl;

    /** URL that the WebAPK should navigate to when launched from the homescreen. */
    public final String manifestStartUrl;

    /**
     * Id field of the Web Manifest. Empty or null means this is from an older version of
     * shell(<155) that did not set this value.
     */
    @Nullable public final String manifestId;

    /**
     * Key of the WebAPK. The value should either be the same as the Manifest URL or the Manifest
     * Unique ID, or empty depending on the situation. Empty or null means this is from an older
     * version of shell (<155) that did not set this value.
     */
    @Nullable public final String appKey;

    /** The source from where the WebAPK is installed. */
    public final @WebApkDistributor int distributor;

    /** Map of the WebAPK's icon URLs to Murmur2 hashes of the icon untransformed bytes. */
    @NonNull public final Map<String, String> iconUrlToMurmur2HashMap;

    /**
     * ShareTarget data
     * TODO(pkotwicz): Remove this property in favor of
     * {@link BrowserServicesIntentDataProvider#shareTarget()}
     */
    @Nullable public final WebApkShareTarget shareTarget;

    /**
     * Whether the WebAPK
     * (1) Launches an internal activity to display the splash screen
     * AND
     * (2) Has a content provider which provides a screenshot of the splash screen.
     */
    public final boolean isSplashProvidedByWebApk;

    /** The list of the WebAPK's shortcuts. */
    @NonNull public final List<ShortcutItem> shortcutItems;

    /** WebAPK's version code. */
    public final int webApkVersionCode;

    /** WebAPK's last update timestamp. */
    public final long lastUpdateTime;

    public final boolean hasCustomName;

    /** A class that stores information from shortcut items. */
    public static class ShortcutItem {
        public String name;
        public String shortName;
        public String launchUrl;
        public String iconUrl;
        public String iconHash;
        public @NonNull WebappIcon icon;

        public ShortcutItem(
                String name,
                String shortName,
                String launchUrl,
                String iconUrl,
                String iconHash,
                @NonNull WebappIcon icon) {
            this.name = name;
            this.shortName = shortName;
            this.launchUrl = launchUrl;
            this.iconUrl = iconUrl;
            this.iconHash = iconHash;
            this.icon = icon;
        }
    }

    public static WebApkExtras createEmpty() {
        return new WebApkExtras(
                /* webApkPackageName= */ null,
                new WebappIcon(),
                /* isSplashIconMaskable= */ false,
                /* shellApkVersion= */ 0,
                /* manifestUrl= */ null,
                /* manifestStartUrl= */ null,
                /* manifestId= */ null,
                /* appKey= */ null,
                WebApkDistributor.OTHER,
                new HashMap<String, String>()
                /* iconUrlToMurmur2HashMap= */ ,
                /* shareTarget= */ null,
                /* isSplashProvidedByWebApk= */ false,
                new ArrayList<>()
                /* shortcutItems= */ ,
                /* webApkVersionCode= */ 0,
                /* lastUpdateTime= */ 0,
                /* hasCustomName= */ false);
    }

    public WebApkExtras(
            String webApkPackageName,
            @NonNull WebappIcon splashIcon,
            boolean isSplashIconMaskable,
            int shellApkVersion,
            String manifestUrl,
            String manifestStartUrl,
            String manifestId,
            String appKey,
            @WebApkDistributor int distributor,
            @NonNull Map<String, String> iconUrlToMurmur2HashMap,
            @Nullable WebApkShareTarget shareTarget,
            boolean isSplashProvidedByWebApk,
            @NonNull List<ShortcutItem> shortcutItems,
            int webApkVersionCode,
            long lastUpdateTime,
            boolean hasCustomName) {
        this.webApkPackageName = webApkPackageName;
        this.splashIcon = splashIcon;
        this.isSplashIconMaskable = isSplashIconMaskable;
        this.shellApkVersion = shellApkVersion;
        this.manifestUrl = manifestUrl;
        this.manifestStartUrl = manifestStartUrl;
        this.manifestId = manifestId;
        this.appKey = appKey;
        this.distributor = distributor;
        this.iconUrlToMurmur2HashMap = iconUrlToMurmur2HashMap;
        this.shareTarget = shareTarget;
        this.isSplashProvidedByWebApk = isSplashProvidedByWebApk;
        this.shortcutItems = shortcutItems;
        this.webApkVersionCode = webApkVersionCode;
        this.lastUpdateTime = lastUpdateTime;
        this.hasCustomName = hasCustomName;
    }
}
