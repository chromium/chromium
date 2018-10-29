// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

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
        mNativePointer = nativeInitialize(mOldInfo.scopeUri().toString(), mOldInfo.manifestUrl());
        nativeStart(mNativePointer, mTab.getWebContents());
        return true;
    }

    /**
     * Puts the object in a state where it is safe to be destroyed.
     */
    public void destroy() {
        mTab.removeObserver(this);
        nativeDestroy(mNativePointer);
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
        nativeReplaceWebContents(mNativePointer, mTab.getWebContents());
    }

    /**
     * Called when the updated Web Manifest has been fetched.
     */
    @CalledByNative
    protected void onDataAvailable(String manifestStartUrl, String scopeUrl, String name,
            String shortName, String primaryIconUrl, String primaryIconMurmur2Hash,
            Bitmap primaryIconBitmap, String badgeIconUrl, String badgeIconMurmur2Hash,
            Bitmap badgeIconBitmap, String[] iconUrls, @WebDisplayMode int displayMode,
            int orientation, long themeColor, long backgroundColor, String shareAction,
            String shareParamsTitle, String shareParamsText, String shareParamsUrl) {
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

        String serializedShareTarget = WebApkInfo.getSerializedShareTarget(
                shareAction, shareParamsTitle, shareParamsText, shareParamsUrl);

        WebApkInfo info = WebApkInfo.create(mOldInfo.id(), mOldInfo.uri().toString(), scopeUrl,
                new WebApkInfo.Icon(primaryIconBitmap), new WebApkInfo.Icon(badgeIconBitmap), null,
                name, shortName, displayMode, orientation, mOldInfo.source(), themeColor,
                backgroundColor, mOldInfo.apkPackageName(), mOldInfo.shellApkVersion(),
                mOldInfo.manifestUrl(), manifestStartUrl, WebApkInfo.WebApkDistributor.BROWSER,
                iconUrlToMurmur2HashMap, serializedShareTarget, mOldInfo.shouldForceNavigation(),
                mOldInfo.useTransparentSplash());
        mObserver.onGotManifestData(info, primaryIconUrl, badgeIconUrl);
    }

    private native long nativeInitialize(String scope, String webManifestUrl);
    private native void nativeReplaceWebContents(
            long nativeWebApkUpdateDataFetcher, WebContents webContents);
    private native void nativeDestroy(long nativeWebApkUpdateDataFetcher);
    private native void nativeStart(long nativeWebApkUpdateDataFetcher, WebContents webContents);
}
