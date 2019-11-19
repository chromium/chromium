// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.chrome.browser.webapps.WebApkInfo.ShareData;
import org.chromium.chrome.browser.webapps.WebApkInfo.ShareTarget;

import java.util.Map;

/**
 * Stores WebAPK specific information on behalf of {@link BrowserServicesIntentDataProvider}.
 */
public class WebApkExtras {
    /**
     * The package of the WebAPK.
     */
    public final String webApkPackageName;

    /**
     * Badge icon to use for notifications.
     */
    public final WebappIcon badgeIcon;

    /**
     * Icon to use for the splash screen.
     */
    public final WebappIcon splashIcon;

    /**
     * Whether the WebAPK's splash icon should be masked.
     */
    public final boolean isSplashIconMaskable;

    /**
     * Version of the code in //chrome/android/webapk/shell_apk.
     */
    public final int shellApkVersion;

    /**
     * URL of the Web Manifest.
     */
    public final String manifestUrl;

    /**
     * URL that the WebAPK should navigate to when launched from the homescreen.
     */
    public final String manifestStartUrl;

    /** The source from where the WebAPK is installed. */
    public final @WebApkDistributor int distributor;

    /**
     * Map of the WebAPK's icon URLs to Murmur2 hashes of the icon untransformed bytes.
     */
    public final Map<String, String> iconUrlToMurmur2HashMap;

    /**
     * ShareTarget data
     * TODO(pkotwicz): Remove this property in favor of
     * {@link BrowserServicesIntentDataProvider#shareTarget()}
     */
    public final ShareTarget shareTarget;

    /**
     * Whether the WebAPK
     * (1) Launches an internal activity to display the splash screen
     * AND
     * (2) Has a content provider which provides a screenshot of the splash screen.
     */
    public final boolean isSplashProvidedByWebApk;

    /**
     * Shared information from the share intent.
     * TODO(pkotwicz): Remove this property in favor of
     * {@link BrowserServicesIntentDataProvider#shareData()}
     */
    public final ShareData shareData;

    /**
     * WebAPK's version code.
     */
    public final int webApkVersionCode;

    public static WebApkExtras createEmpty() {
        return new WebApkExtras(null /* webApkPackageName */, new WebappIcon(), new WebappIcon(),
                false /* isSplashIconMaskable */, 0 /* shellApkVersion */, null /* manifestUrl */,
                null /* manifestStartUrl */, WebApkDistributor.OTHER,
                null /* iconUrlToMurmur2HashMap */, new ShareTarget(),
                false /* isSplashProvidedByWebApk */, null /* shareData */,
                0 /* webApkVersionCode */);
    }

    public WebApkExtras(String webApkPackageName, WebappIcon badgeIcon, WebappIcon splashIcon,
            boolean isSplashIconMaskable, int shellApkVersion, String manifestUrl,
            String manifestStartUrl, @WebApkDistributor int distributor,
            Map<String, String> iconUrlToMurmur2HashMap, ShareTarget shareTarget,
            boolean isSplashProvidedByWebApk, ShareData shareData, int webApkVersionCode) {
        this.webApkPackageName = webApkPackageName;
        this.badgeIcon = badgeIcon;
        this.splashIcon = splashIcon;
        this.isSplashIconMaskable = isSplashIconMaskable;
        this.shellApkVersion = shellApkVersion;
        this.manifestUrl = manifestUrl;
        this.manifestStartUrl = manifestStartUrl;
        this.distributor = distributor;
        this.iconUrlToMurmur2HashMap = iconUrlToMurmur2HashMap;
        this.shareTarget = shareTarget;
        this.isSplashProvidedByWebApk = isSplashProvidedByWebApk;
        this.shareData = shareData;
        this.webApkVersionCode = webApkVersionCode;
    }
}
