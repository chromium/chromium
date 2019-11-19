// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.webapk.lib.common.splash.SplashLayout;

import java.util.HashMap;

/**
 * Downloads the Web Manifest if the web site still uses the {@link manifestUrl} passed to the
 * constructor.
 */
public class WebApkUpdateDataFetcher extends EmptyTabObserver {
    /** Observes fetching of the Web Manifest. */
    public interface Observer {
        /**
         * Called when the Web Manifest has been successfully fetched (including on the initial URL
         * load).
         * @param fetchedInfo    The fetched Web Manifest data.
         * @param primaryIconUrl The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
         *                       suited for use as the launcher icon on this device.
         * @param badgeIconUrl   The icon URL in {@link fetchedInfo#iconUrlToMurmur2HashMap()} best
         *                       suited for use as the badge icon on this device.
         */
        void onGotManifestData(WebApkInfo fetchedInfo, String primaryIconUrl, String badgeIconUrl);
    }

    /**
     * Pointer to the native side WebApkUpdateDataFetcher. The Java side owns the native side
     * WebApkUpdateDataFetcher.
     */
    private long mNativePointer;

    /** The tab that is being observed. */
    private Tab mTab;

    /** Web Manifest data at the time that the WebAPK was generated. */
    private WebApkInfo mOldInfo;

    private Observer mObserver;

    /** Starts observing page loads in order to fetch the Web Manifest after each page load. */
    public boolean start(Tab tab, WebApkInfo oldInfo, Observer observer) {
        if (tab.getWebContents() == null || TextUtils.isEmpty(oldInfo.manifestUrl())) {
            return false;
        }

        mTab = tab;
        mOldInfo = oldInfo;
        mObserver = observer;

        mTab.addObserver(this);
        mNativePointer = WebApkUpdateDataFetcherJni.get().initialize(
                WebApkUpdateDataFetcher.this, mOldInfo.scopeUrl(), mOldInfo.manifestUrl());
        WebApkUpdateDataFetcherJni.get().start(
                mNativePointer, WebApkUpdateDataFetcher.this, mTab.getWebContents());
        return true;
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        mTab.removeObserver(this);
        WebApkUpdateDataFetcherJni.get().destroy(mNativePointer, WebApkUpdateDataFetcher.this);
        mNativePointer = 0;
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad,
            boolean didFinishLoad) {
        updatePointers();
    }

    @Override
    public void onContentChanged(Tab tab) {
        updatePointers();
    }

    /**
     * Updates which WebContents the native WebApkUpdateDataFetcher is monitoring.
     */
    private void updatePointers() {
        WebApkUpdateDataFetcherJni.get().replaceWebContents(
                mNativePointer, WebApkUpdateDataFetcher.this, mTab.getWebContents());
    }

    /**
     * Called when the updated Web Manifest has been fetched.
     */
    @CalledByNative
    protected void onDataAvailable(String manifestStartUrl, String scopeUrl, String name,
            String shortName, String primaryIconUrl, String primaryIconMurmur2Hash,
            Bitmap primaryIconBitmap, boolean isPrimaryIconMaskable, String badgeIconUrl,
            String badgeIconMurmur2Hash, Bitmap badgeIconBitmap, String[] iconUrls,
            @WebDisplayMode int displayMode, int orientation, long themeColor, long backgroundColor,
            String shareAction, String shareParamsTitle, String shareParamsText,
            boolean isShareMethodPost, boolean isShareEncTypeMultipart,
            String[] shareParamsFileNames, String[][] shareParamsAccepts) {
        Context appContext = ContextUtils.getApplicationContext();

        HashMap<String, String> iconUrlToMurmur2HashMap = new HashMap<String, String>();
        for (String iconUrl : iconUrls) {
            String murmur2Hash = null;
            if (iconUrl.equals(primaryIconUrl)) {
                murmur2Hash = primaryIconMurmur2Hash;
            } else if (iconUrl.equals(badgeIconUrl)) {
                murmur2Hash = badgeIconMurmur2Hash;
            }
            iconUrlToMurmur2HashMap.put(iconUrl, murmur2Hash);
        }

        // When share action is empty, we use a default empty share target
        WebApkInfo.ShareTarget shareTarget = TextUtils.isEmpty(shareAction)
                ? new WebApkInfo.ShareTarget()
                : new WebApkInfo.ShareTarget(shareAction, shareParamsTitle, shareParamsText,
                        isShareMethodPost, isShareEncTypeMultipart, shareParamsFileNames,
                        shareParamsAccepts);

        int defaultBackgroundColor = SplashLayout.getDefaultBackgroundColor(appContext);
        WebApkInfo info = WebApkInfo.create(mOldInfo.url(), scopeUrl,
                new WebappIcon(primaryIconBitmap), new WebappIcon(badgeIconBitmap), null, name,
                shortName, displayMode, orientation, mOldInfo.source(), themeColor, backgroundColor,
                defaultBackgroundColor, isPrimaryIconMaskable, false /* isSplashIconMaskable */,
                mOldInfo.webApkPackageName(), mOldInfo.shellApkVersion(), mOldInfo.manifestUrl(),
                manifestStartUrl, WebApkDistributor.BROWSER, iconUrlToMurmur2HashMap, shareTarget,
                mOldInfo.shouldForceNavigation(), mOldInfo.isSplashProvidedByWebApk(), null,
                mOldInfo.webApkVersionCode());
        mObserver.onGotManifestData(info, primaryIconUrl, badgeIconUrl);
    }

    @NativeMethods
    interface Natives {
        long initialize(WebApkUpdateDataFetcher caller, String scope, String webManifestUrl);
        void replaceWebContents(long nativeWebApkUpdateDataFetcher, WebApkUpdateDataFetcher caller,
                WebContents webContents);
        void destroy(long nativeWebApkUpdateDataFetcher, WebApkUpdateDataFetcher caller);
        void start(long nativeWebApkUpdateDataFetcher, WebApkUpdateDataFetcher caller,
                WebContents webContents);
    }
}
