// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebApkExtras;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Downloads the Web Manifest if the web site still uses the {@link manifestUrl} passed to the
 * constructor.
 */
@NullMarked
public class WebApkUpdateDataFetcher extends EmptyTabObserver {
    /** Observes fetching of the Web Manifest. */
    public interface Observer {
        /**
         * Called when the Web Manifest has been successfully fetched (including on the initial URL
         * load).
         *
         * @param fetchedInfo The fetched Web Manifest data.
         * @param primaryIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
         *     suited for use as the launcher icon on this device.
         * @param splashIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
         *     suited for use as the splash icon on this device.
         */
        void onGotManifestData(
                @Nullable BrowserServicesIntentDataProvider fetchedInfo,
                @Nullable String primaryIconUrl,
                @Nullable String splashIconUrl);
    }

    /**
     * Pointer to the native side WebApkUpdateDataFetcher. The Java side owns the native side
     * WebApkUpdateDataFetcher.
     */
    private long mNativePointer;

    /** The tab that is being observed. */
    private @Nullable Tab mTab;

    /** Web Manifest data at the time that the WebAPK was generated. */
    private @Nullable WebappInfo mOldInfo;

    private @Nullable Observer mObserver;

    /** Starts observing page loads in order to fetch the Web Manifest after each page load. */
    public boolean start(Tab tab, WebappInfo oldInfo, Observer observer) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null || TextUtils.isEmpty(oldInfo.manifestUrl())) {
            return false;
        }

        mTab = tab;
        mOldInfo = oldInfo;
        mObserver = observer;

        tab.addObserver(this);
        mNativePointer =
                WebApkUpdateDataFetcherJni.get()
                        .initialize(
                                this,
                                oldInfo.manifestStartUrl(),
                                oldInfo.scopeUrl(),
                                oldInfo.manifestUrl(),
                                oldInfo.manifestId());
        WebApkUpdateDataFetcherJni.get().start(mNativePointer, webContents);
        return true;
    }

    /** Puts the object in a state where it is safe to be destroyed. */
    public void destroy() {
        if (mTab == null) return;
        mTab.removeObserver(this);
        WebApkUpdateDataFetcherJni.get().destroy(mNativePointer);
        mNativePointer = 0;
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePointers();
    }

    /** Updates which WebContents the native WebApkUpdateDataFetcher is monitoring. */
    private void updatePointers() {
        assumeNonNull(mTab);
        WebApkUpdateDataFetcherJni.get().replaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /** Called when the updated Web Manifest has been fetched. */
    @CalledByNative
    protected void onDataAvailable(
            @JniType("std::string") String manifestStartUrl,
            @JniType("std::string") String scopeUrl,
            @JniType("std::u16string") String name,
            @JniType("std::u16string") String shortName,
            @JniType("std::string") String manifestUrl,
            String manifestId,
            @JniType("std::string") String primaryIconUrl,
            @JniType("std::string") String primaryIconMurmur2Hash,
            Bitmap primaryIconBitmap,
            boolean isPrimaryIconMaskable,
            @JniType("std::string") String splashIconUrl,
            @JniType("std::string") String splashIconMurmur2Hash,
            byte[] splashIconData,
            boolean isSplashIconMaskable,
            String[] iconUrls,
            @DisplayMode.EnumType int displayMode,
            int orientation,
            long themeColor,
            long backgroundColor,
            long darkThemeColor,
            long darkBackgroundColor,
            @JniType("std::string") String shareAction,
            @JniType("std::u16string") String shareParamsTitle,
            @JniType("std::u16string") String shareParamsText,
            boolean isShareMethodPost,
            boolean isShareEncTypeMultipart,
            String[] shareParamsFileNames,
            String[][] shareParamsAccepts,
            String[][] shortcuts,
            byte[][] shortcutIconData) {
        Context appContext = ContextUtils.getApplicationContext();

        HashMap<String, String> iconUrlToMurmur2HashMap = new HashMap<>();
        for (String iconUrl : iconUrls) {
            String murmur2Hash = null;
            if (iconUrl.equals(primaryIconUrl)) {
                murmur2Hash = primaryIconMurmur2Hash;
            } else if (iconUrl.equals(splashIconUrl)) {
                murmur2Hash = splashIconMurmur2Hash;
            }
            iconUrlToMurmur2HashMap.put(iconUrl, murmur2Hash);
        }

        List<WebApkExtras.ShortcutItem> shortcutItems = new ArrayList<>();
        for (int i = 0; i < shortcuts.length; i++) {
            String[] shortcutData = shortcuts[i];
            shortcutItems.add(
                    new WebApkExtras.ShortcutItem(
                            /* name= */ shortcutData[0],
                            /* shortName= */ shortcutData[1],
                            /* launchUrl= */ shortcutData[2],
                            /* iconUrl= */ shortcutData[3],
                            /* iconHash= */ shortcutData[4],
                            new WebappIcon(shortcutIconData[i])));
        }

        // When share action is empty, we use a default empty share target
        WebApkShareTarget shareTarget = null;
        if (!TextUtils.isEmpty(shareAction)) {
            shareTarget =
                    new WebApkShareTarget(
                            shareAction,
                            shareParamsTitle,
                            shareParamsText,
                            isShareMethodPost,
                            isShareEncTypeMultipart,
                            shareParamsFileNames,
                            shareParamsAccepts);
        }

        assumeNonNull(mOldInfo);
        assumeNonNull(mObserver);
        int defaultBackgroundColor = SplashLayout.getDefaultBackgroundColor(appContext);
        BrowserServicesIntentDataProvider intentDataProvider =
                WebApkIntentDataProviderFactory.create(
                        new Intent(),
                        mOldInfo.url(),
                        scopeUrl,
                        new WebappIcon(primaryIconBitmap),
                        new WebappIcon(splashIconData),
                        name,
                        shortName,
                        mOldInfo.hasCustomName(),
                        displayMode,
                        orientation,
                        mOldInfo.source(),
                        themeColor,
                        backgroundColor,
                        darkThemeColor,
                        darkBackgroundColor,
                        defaultBackgroundColor,
                        isPrimaryIconMaskable,
                        isSplashIconMaskable,
                        mOldInfo.webApkPackageName(),
                        mOldInfo.shellApkVersion(),
                        manifestUrl,
                        manifestStartUrl,
                        manifestId,
                        mOldInfo.appKey(),
                        WebApkDistributor.BROWSER,
                        iconUrlToMurmur2HashMap,
                        shareTarget,
                        mOldInfo.shouldForceNavigation(),
                        mOldInfo.isSplashProvidedByWebApk(),
                        null,
                        shortcutItems,
                        mOldInfo.webApkVersionCode(),
                        mOldInfo.lastUpdateTime());
        mObserver.onGotManifestData(intentDataProvider, primaryIconUrl, splashIconUrl);
    }

    @NativeMethods
    interface Natives {
        long initialize(
                WebApkUpdateDataFetcher self,
                @Nullable @JniType("std::string") String startUrl,
                @Nullable @JniType("std::string") String scope,
                @Nullable @JniType("std::string") String webManifestUrl,
                @Nullable String webManifestId);

        void replaceWebContents(
                long nativeWebApkUpdateDataFetcher, @Nullable WebContents webContents);

        void destroy(long nativeWebApkUpdateDataFetcher);

        void start(long nativeWebApkUpdateDataFetcher, WebContents webContents);
    }
}
