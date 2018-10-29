// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.SparseArray;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.content.TitleBitmapFactory;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.BitmapDynamicResource;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * A version of the {@link LayerTitleCache} that builds native cc::Layer objects
 * that represent the cached title textures.
 */
@JNINamespace("android")
public class LayerTitleCache implements TitleCache {
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

    /**
     * Builds an instance of the LayerTitleCache.
     */
    public LayerTitleCache(Context context) {
        mContext = context;
        Resources res = context.getResources();
        final int fadeWidthPx = res.getDimensionPixelOffset(R.dimen.border_texture_title_fade);
        final int faviconStartPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_start_padding);
        final int faviconEndPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_end_padding);
        mNativeLayerTitleCache = nativeInit(fadeWidthPx, faviconStartPaddingPx, faviconEndPaddingPx,
                R.drawable.spinner, R.drawable.spinner_white);
        mFaviconSize = res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mStandardTitleBitmapFactory = new TitleBitmapFactory(context, false);
        mDarkTitleBitmapFactory = new TitleBitmapFactory(context, true);
        mDefaultFaviconHelper = new DefaultFaviconHelper();
    }

    /**
     * @param resourceManager The {@link ResourceManager} for registering title
     *                        resources.
     */
    public void setResourceManager(ResourceManager resourceManager) {
        mResourceManager = resourceManager;
    }

    /**
     * Destroys the native reference.
     */
    public void shutDown() {
        if (mNativeLayerTitleCache == 0) return;
        nativeDestroy(mNativeLayerTitleCache);
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
        if (tab == null) return;

        getUpdatedTitle(tab, "");
    }

    @Override
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

    private String getUpdatedTitleInternal(Tab tab, String titleString,
            boolean fetchFaviconFromHistory) {
        final int tabId = tab.getId();
        boolean isHTSEnabled = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(tab.getActivity())
                && ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
        boolean isDarkTheme = tab.isIncognito() && !isHTSEnabled;
        Bitmap originalFavicon = tab.getFavicon();
        if (originalFavicon == null) {
            originalFavicon = mDefaultFaviconHelper.getDefaultFaviconBitmap(
                    mContext, tab.getUrl(), !isDarkTheme);
        }

        boolean isRtl = tab.isTitleDirectionRtl();
        TitleBitmapFactory titleBitmapFactory =
                isDarkTheme ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        Title title = mTitles.get(tabId);
        if (title == null) {
            title = new Title();
            mTitles.put(tabId, title);
            title.register();
        }

        title.set(titleBitmapFactory.getTitleBitmap(mContext, titleString),
                titleBitmapFactory.getFaviconBitmap(originalFavicon), fetchFaviconFromHistory);

        if (mNativeLayerTitleCache != 0) {
            nativeUpdateLayer(mNativeLayerTitleCache, tabId, title.getTitleResId(),
                    title.getFaviconResId(), isDarkTheme, isRtl);
        }
        return titleString;
    }

    private void fetchFaviconForTab(final Tab tab) {
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();

        // Since tab#getProfile() is not available by this time, we will use whatever last used
        // profile. This should be normal profile since fetching favicons should normally happen on
        // a cold start. Return otherwise.
        if (Profile.getLastUsedProfile().hasOffTheRecordProfile()) return;

        mFaviconHelper.getLocalFaviconImageForURL(
                Profile.getLastUsedProfile(),
                tab.getUrl(),
                mFaviconSize,
                new FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap favicon, String iconUrl) {
                        updateFaviconFromHistory(tab, favicon);
                    }
                });
    }

    /**
     * Comes up with a valid title to return for a tab.
     * @param tab The {@link Tab} to build a title for.
     * @return    The title to use.
     */
    private String getTitleForTab(Tab tab, String defaultTitle) {
        String title = tab.getTitle();
        if (TextUtils.isEmpty(title)) {
            title = tab.getUrl();
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
            nativeUpdateFavicon(mNativeLayerTitleCache, tabId, title.getFaviconResId());
        }
    }

    @Override
    public void remove(int tabId) {
        Title title = mTitles.get(tabId);
        if (title == null) return;
        title.unregister();
        mTitles.remove(tabId);
        if (mNativeLayerTitleCache == 0) return;
        nativeUpdateLayer(mNativeLayerTitleCache, tabId, -1, -1, false, false);
    }

    @Override
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
        nativeClearExcept(mNativeLayerTitleCache, exceptId);
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

    private native long nativeInit(int fadeWidth, int faviconStartlPadding, int faviconEndPadding,
            int spinnerResId, int spinnerIncognitoResId);
    private static native void nativeDestroy(long nativeLayerTitleCache);
    private native void nativeClearExcept(long nativeLayerTitleCache, int exceptId);
    private native void nativeUpdateLayer(long nativeLayerTitleCache, int tabId, int titleResId,
            int faviconResId, boolean isIncognito, boolean isRtl);
    private native void nativeUpdateFavicon(long nativeLayerTitleCache, int tabId,
            int faviconResId);
}
