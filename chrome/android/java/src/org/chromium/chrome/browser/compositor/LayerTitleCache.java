// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.SparseArray;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TitleBitmapFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.BitmapDynamicResource;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * A version of the {@link LayerTitleCache} that builds native cc::Layer objects
 * that represent the cached title textures.
 */
@JNINamespace("android")
public class LayerTitleCache {
    private static int sNextResourceId = 1;

    private final Context mContext;
    private TabModelSelector mTabModelSelector;

    private final SparseArray<Title> mTitles = new SparseArray<Title>();
    private final int mFaviconSize;

    private long mNativeLayerTitleCache;
    private ResourceManager mResourceManager;

    private FaviconHelper mFaviconHelper;
    private DefaultFaviconHelper mDefaultFaviconHelper;

    /** Responsible for building titles on light themes or standard tabs. */
    protected TitleBitmapFactory mStandardTitleBitmapFactory;

    /** Responsible for building incognito or dark theme titles. */
    protected TitleBitmapFactory mDarkTitleBitmapFactory;

    /** Builds an instance of the LayerTitleCache. */
    public LayerTitleCache(Context context, ResourceManager resourceManager) {
        mContext = context;
        mResourceManager = resourceManager;
        Resources res = context.getResources();
        final int fadeWidthPx = res.getDimensionPixelOffset(R.dimen.border_texture_title_fade);
        final int faviconStartPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_start_padding);
        final int faviconEndPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_end_padding);
        mNativeLayerTitleCache =
                LayerTitleCacheJni.get()
                        .init(
                                LayerTitleCache.this,
                                fadeWidthPx,
                                faviconStartPaddingPx,
                                faviconEndPaddingPx,
                                R.drawable.spinner,
                                R.drawable.spinner_white,
                                mResourceManager);
        mFaviconSize = res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mStandardTitleBitmapFactory = new TitleBitmapFactory(context, false);
        mDarkTitleBitmapFactory = new TitleBitmapFactory(context, true);
        mDefaultFaviconHelper = new DefaultFaviconHelper();
    }

    /** Destroys the native reference. */
    public void shutDown() {
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get().destroy(mNativeLayerTitleCache);
        mNativeLayerTitleCache = 0;
    }

    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativeLayerTitleCache;
    }

    @CalledByNative
    private void buildUpdatedTitle(int tabId) {
        if (mTabModelSelector == null) return;

        Tab tab = mTabModelSelector.getTabById(tabId);
        if (tab == null || tab.isDestroyed()) return;

        getUpdatedTitle(tab, "");
    }

    public String getUpdatedTitle(Tab tab, String defaultTitle) {
        // If content view core is null, tab does not have direct access to the favicon, and we
        // will initially show default favicon. But favicons are stored in the history database, so
        // we will fetch favicons asynchronously from database.
        boolean fetchFaviconFromHistory = tab.isNativePage() || tab.getWebContents() == null;

        String titleString = getTitleForTab(tab, defaultTitle);
        getUpdatedTitleInternal(tab, titleString, fetchFaviconFromHistory);
        if (fetchFaviconFromHistory) fetchFaviconForTab(tab);
        return titleString;
    }

    private String getUpdatedTitleInternal(
            Tab tab, String titleString, boolean fetchFaviconFromHistory) {
        final int tabId = tab.getId();
        boolean isDarkTheme = tab.isIncognito();
        Bitmap originalFavicon = getOriginalFavicon(tab);

        TitleBitmapFactory titleBitmapFactory =
                isDarkTheme ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        Title title = mTitles.get(tabId);
        if (title == null) {
            title = new Title();
            mTitles.put(tabId, title);
            title.register();
        }

        title.set(
                titleBitmapFactory.getTitleBitmap(mContext, titleString),
                titleBitmapFactory.getFaviconBitmap(originalFavicon),
                fetchFaviconFromHistory);

        if (mNativeLayerTitleCache != 0) {
            String tabTitle = tab.getTitle();
            boolean isRtl =
                    tabTitle != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(tabTitle)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateLayer(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            tabId,
                            title.getTitleResId(),
                            title.getFaviconResId(),
                            isDarkTheme,
                            isRtl);
        }
        return titleString;
    }

    private void fetchFaviconForTab(final Tab tab) {
        fetchFaviconWithCallback(tab, (favicon, iconUrl) -> updateFaviconFromHistory(tab, favicon));
    }

    /**
     * Requests the favicon for the given tab.
     *
     * @param tab The {@link Tab} to request the favicon for.
     * @param callback A callback to run when the favicon is available.
     */
    public void fetchFaviconWithCallback(final Tab tab, FaviconImageCallback callback) {
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();

        // Since tab#getProfile() is not available by this time, we will use tab#isIncognito boolean
        // to get the correct profile.
        Profile profile =
                !tab.isIncognito()
                        ? Profile.getLastUsedRegularProfile()
                        : Profile.getLastUsedRegularProfile()
                                .getPrimaryOTRProfile(/* createIfNeeded= */ true);
        mFaviconHelper.getLocalFaviconImageForURL(profile, tab.getUrl(), mFaviconSize, callback);
    }

    /**
     * Requests a default favicon for the given tab.
     *
     * @param tab The {@link Tab} to request the favicon for.
     * @return The tab's favicon based on its web contents. Otherwise, a default favicon.
     */
    public Bitmap getOriginalFavicon(Tab tab) {
        boolean isDarkTheme = tab.isIncognito();
        Bitmap originalFavicon = TabFavicon.getBitmap(tab);
        if (originalFavicon == null) {
            originalFavicon =
                    mDefaultFaviconHelper.getDefaultFaviconBitmap(
                            mContext, tab.getUrl(), !isDarkTheme);
        }

        return originalFavicon;
    }

    /**
     * Comes up with a valid title to return for a tab.
     * @param tab The {@link Tab} to build a title for.
     * @return    The title to use.
     */
    private String getTitleForTab(Tab tab, String defaultTitle) {
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) {
            title = tab.getUrl().getSpec();
            if (TextUtils.isEmpty(title)) {
                title = defaultTitle;
                if (TextUtils.isEmpty(title)) {
                    title = "";
                }
            }
        }
        return title;
    }

    private void updateFaviconFromHistory(Tab tab, Bitmap faviconBitmap) {
        if (!tab.isInitialized()) return;

        int tabId = tab.getId();
        Title title = mTitles.get(tabId);
        if (title == null) return;
        if (!title.updateFaviconFromHistory(faviconBitmap)) return;

        if (mNativeLayerTitleCache != 0) {
            LayerTitleCacheJni.get()
                    .updateFavicon(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            tabId,
                            title.getFaviconResId());
        }
    }

    public void remove(int tabId) {
        Title title = mTitles.get(tabId);
        if (title == null) return;
        title.unregister();
        mTitles.remove(tabId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateLayer(
                        mNativeLayerTitleCache, LayerTitleCache.this, tabId, -1, -1, false, false);
    }

    public void clearExcept(int exceptId) {
        Title title = mTitles.get(exceptId);
        for (int i = 0; i < mTitles.size(); i++) {
            Title toDelete = mTitles.get(mTitles.keyAt(i));
            if (toDelete == title) continue;
            toDelete.unregister();
        }
        mTitles.clear();
        mDefaultFaviconHelper.clearCache();

        if (title != null) mTitles.put(exceptId, title);

        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .clearExcept(mNativeLayerTitleCache, LayerTitleCache.this, exceptId);
    }

    private class Title {
        private final BitmapDynamicResource mFavicon = new BitmapDynamicResource(sNextResourceId++);
        private final BitmapDynamicResource mTitle = new BitmapDynamicResource(sNextResourceId++);

        // We don't want to override updated favicon (e.g. from Tab#onFaviconAvailable) with one
        // fetched from history. You can set this to true / false to control that.
        private boolean mExpectUpdateFromHistory;

        public Title() {}

        public void set(Bitmap titleBitmap, Bitmap faviconBitmap, boolean expectUpdateFromHistory) {
            mTitle.setBitmap(titleBitmap);
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = expectUpdateFromHistory;
        }

        public boolean updateFaviconFromHistory(Bitmap faviconBitmap) {
            if (!mExpectUpdateFromHistory) return false;
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = false;
            return true;
        }

        public void register() {
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mFavicon.getResId(), mFavicon);
            loader.registerResource(mTitle.getResId(), mTitle);
        }

        public void unregister() {
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mFavicon.getResId());
            loader.unregisterResource(mTitle.getResId());
        }

        public int getFaviconResId() {
            return mFavicon.getResId();
        }

        public int getTitleResId() {
            return mTitle.getResId();
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                LayerTitleCache caller,
                int fadeWidth,
                int faviconStartlPadding,
                int faviconEndPadding,
                int spinnerResId,
                int spinnerIncognitoResId,
                ResourceManager resourceManager);

        void destroy(long nativeLayerTitleCache);

        void clearExcept(long nativeLayerTitleCache, LayerTitleCache caller, int exceptId);

        void updateLayer(
                long nativeLayerTitleCache,
                LayerTitleCache caller,
                int tabId,
                int titleResId,
                int faviconResId,
                boolean isIncognito,
                boolean isRtl);

        void updateFavicon(
                long nativeLayerTitleCache, LayerTitleCache caller, int tabId, int faviconResId);
    }
}
