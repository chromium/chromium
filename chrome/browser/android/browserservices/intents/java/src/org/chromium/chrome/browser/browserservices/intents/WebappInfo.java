// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.sharing.ShareData;

import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras.ShortcutItem;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;

import java.util.List;
import java.util.Map;

/** Stores info about a web app. */
public class WebappInfo {
    private final @NonNull BrowserServicesIntentDataProvider mProvider;

    // Initialized lazily by {@link getWebApkExtras()}.
    private @Nullable WebApkExtras mWebApkExtras;

    /**
     * Construct a WebappInfo.
     * @param intent Intent containing info about the app.
     */
    public static WebappInfo create(@Nullable BrowserServicesIntentDataProvider provider) {
        return (provider == null) ? null : new WebappInfo(provider);
    }

    protected WebappInfo(@NonNull BrowserServicesIntentDataProvider provider) {
        mProvider = provider;
    }

    public @NonNull BrowserServicesIntentDataProvider getProvider() {
        return mProvider;
    }

    public String id() {
        return getWebappExtras().id;
    }

    public String url() {
        return getWebappExtras().url;
    }

    /**
     * Whether the webapp should be navigated to {@link url()} if the webapp is already open when
     * Chrome receives a ACTION_START_WEBAPP intent.
     */
    public boolean shouldForceNavigation() {
        return getWebappExtras().shouldForceNavigation;
    }

    public String scopeUrl() {
        return getWebappExtras().scopeUrl;
    }

    public String name() {
        return getWebappExtras().name;
    }

    public String shortName() {
        return getWebappExtras().shortName;
    }

    public @DisplayMode.EnumType int displayMode() {
        return getWebappExtras().displayMode;
    }

    public boolean isForWebApk() {
        return !TextUtils.isEmpty(webApkPackageName());
    }

    public String webApkPackageName() {
        return getWebApkExtras().webApkPackageName;
    }

    public @ScreenOrientationLockType.EnumType int orientation() {
        return getWebappExtras().orientation;
    }

    public int source() {
        return getWebappExtras().source;
    }

    /**
     * Returns the toolbar color if it is valid, and
     * ColorUtils.INVALID_COLOR otherwise.
     */
    public long toolbarColor() {
        return hasValidToolbarColor()
                ? mProvider.getLightColorProvider().getToolbarColor()
                : ColorUtils.INVALID_COLOR;
    }

    /** Returns whether the toolbar color specified in the Intent is valid. */
    public boolean hasValidToolbarColor() {
        return mProvider.getLightColorProvider().hasCustomToolbarColor();
    }

    /**
     * Background color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. Return value is a long because we also need to encode the
     * error state of ColorUtils.INVALID_COLOR.
     */
    public long backgroundColor() {
        return WebappIntentUtils.colorFromIntegerColor(getWebappExtras().backgroundColor);
    }

    /** Returns whether the background color specified in the Intent is valid. */
    public boolean hasValidBackgroundColor() {
        return getWebappExtras().backgroundColor != null;
    }

    /**
     * Returns the dark toolbar color if it is valid, and
     * ColorUtils.INVALID_COLOR otherwise.
     */
    public long darkToolbarColor() {
        return hasValidDarkToolbarColor()
                ? mProvider.getDarkColorProvider().getToolbarColor()
                : ColorUtils.INVALID_COLOR;
    }

    /** Returns whether the dark toolbar color specified in the Intent is valid. */
    public boolean hasValidDarkToolbarColor() {
        return mProvider.getDarkColorProvider().hasCustomToolbarColor();
    }

    /**
     * Dark background color is actually a 32 bit unsigned integer which encodes a color
     * in ARGB format. Return value is a long because we also need to encode the
     * error state of ColorUtils.INVALID_COLOR.
     */
    public long darkBackgroundColor() {
        return WebappIntentUtils.colorFromIntegerColor(getWebappExtras().darkBackgroundColor);
    }

    /** Returns whether the dark background color specified in the Intent is valid. */
    public boolean hasValidDarkBackgroundColor() {
        return getWebappExtras().darkBackgroundColor != null;
    }

    /**
     * Returns the background color specified by {@link #backgroundColor()} if
     * the value is valid. Returns the webapp's default background color otherwise.
     */
    public int backgroundColorFallbackToDefault() {
        Integer backgroundColor = getWebappExtras().backgroundColor;
        return (backgroundColor == null)
                ? getWebappExtras().defaultBackgroundColor
                : backgroundColor.intValue();
    }

    /** Returns the icon. */
    public @NonNull WebappIcon icon() {
        return getWebappExtras().icon;
    }

    /** Returns whether the {@link #icon} should be used as an Android Adaptive Icon. */
    public boolean isIconAdaptive() {
        return getWebappExtras().isIconAdaptive;
    }

    /** Returns whether the icon was generated by Chromium. */
    public boolean isIconGenerated() {
        return getWebappExtras().isIconGenerated;
    }

    /**
     * Returns Whether the WebAPK (1) launches an internal activity to display the splash screen
     * and (2) has a content provider which provides a screenshot of the splash screen.
     */
    public boolean isSplashProvidedByWebApk() {
        return getWebApkExtras().isSplashProvidedByWebApk;
    }

    /** Returns the WebAPK's splash icon. */
    public @NonNull WebappIcon splashIcon() {
        return getWebApkExtras().splashIcon;
    }

    public boolean isSplashIconMaskable() {
        return getWebApkExtras().isSplashIconMaskable;
    }

    /** Returns data about the WebAPK's share intent handlers. */
    public @NonNull WebApkShareTarget shareTarget() {
        return getWebApkExtras().shareTarget;
    }

    /** Returns the WebAPK's version code. */
    public int webApkVersionCode() {
        return getWebApkExtras().webApkVersionCode;
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

    /**
     * Return the WebAPK's manifest ID. This returns null for legacy WebAPK (shell version <155).
     */
    @Nullable
    public String manifestId() {
        return getWebApkExtras().manifestId;
    }

    /**
     * Return the WebAPK's manifest ID. This returns the fallback (start url) for legacy WebAPKs
     * (shell version <155). Note that this can still be null if start_url is empty.
     */
    @Nullable
    public String manifestIdWithFallback() {
        return TextUtils.isEmpty(manifestId()) ? manifestStartUrl() : manifestId();
    }

    public String appKey() {
        return getWebApkExtras().appKey;
    }

    public @WebApkDistributor int distributor() {
        return getWebApkExtras().distributor;
    }

    @NonNull
    public Map<String, String> iconUrlToMurmur2HashMap() {
        return getWebApkExtras().iconUrlToMurmur2HashMap;
    }

    public ShareData shareData() {
        return mProvider.getShareData();
    }

    @NonNull
    public List<ShortcutItem> shortcutItems() {
        return getWebApkExtras().shortcutItems;
    }

    public long lastUpdateTime() {
        return getWebApkExtras().lastUpdateTime;
    }

    public boolean hasCustomName() {
        return getWebApkExtras().hasCustomName;
    }

    /**
     * Returns true if the WebappInfo was created for an Intent fired from a launcher shortcut (as
     * opposed to an intent from a push notification or other internal source).
     */
    public boolean isLaunchedFromHomescreen() {
        int source = source();
        return source != ShortcutSource.NOTIFICATION
                && source != ShortcutSource.EXTERNAL_INTENT
                && source != ShortcutSource.EXTERNAL_INTENT_FROM_CHROME
                && source != ShortcutSource.WEBAPK_SHARE_TARGET
                && source != ShortcutSource.WEBAPK_SHARE_TARGET_FILE;
    }

    protected WebappExtras getWebappExtras() {
        WebappExtras extras = mProvider.getWebappExtras();
        assert extras != null;
        return extras;
    }

    protected @NonNull WebApkExtras getWebApkExtras() {
        if (mWebApkExtras != null) return mWebApkExtras;

        mWebApkExtras = mProvider.getWebApkExtras();
        if (mWebApkExtras == null) {
            mWebApkExtras = WebApkExtras.createEmpty();
        }
        return mWebApkExtras;
    }
}
