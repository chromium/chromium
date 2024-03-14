// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.SparseArray;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TitleBitmapFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
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

    private final SparseArray<FaviconTitle> mTabTitles = new SparseArray<FaviconTitle>();
    private final SparseArray<Title> mGroupTitles = new SparseArray<Title>();
    private final int mFaviconSize;

    private long mNativeLayerTitleCache;
    private ResourceManager mResourceManager;

    private FaviconHelper mFaviconHelper;
    private DefaultFaviconHelper mDefaultFaviconHelper;

    private ObserverList<GroupTitleObserver> mGroupTitleObservers = new ObserverList<>();

    /** Responsible for building titles on light themes or standard tabs. */
    protected TitleBitmapFactory mStandardTitleBitmapFactory;

    /** Responsible for building incognito or dark theme titles. */
    protected TitleBitmapFactory mDarkTitleBitmapFactory;

    /** Observer for tab group title layers. */
    public interface GroupTitleObserver {
        /**
         * This method will be called when the result group title bitmap is ready.
         *
         * @param incognito Whether the title's source group is Incognito or not.
         * @param rootId The title's source group's root ID.
         * @param title The text the group title bitmap represents.
         * @param widthPx The width of the title in px.
         */
        void onGroupTitleUpdated(boolean incognito, int rootId, String title, int widthPx);
    }

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

        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) {
            title = new FaviconTitle();
            mTabTitles.put(tabId, title);
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

    @CalledByNative
    private void buildUpdatedGroupTitle(int groupRootId) {
        getUpdatedGroupTitle(groupRootId, "");
    }

    public String getUpdatedGroupTitle(int groupRootId, String defaultTitle) {
        String titleString = TabGroupTitleUtils.getTabGroupTitle(groupRootId);
        if (titleString == null) titleString = defaultTitle;

        getUpdatedGroupTitleInternal(groupRootId, titleString);
        return titleString;
    }

    private String getUpdatedGroupTitleInternal(int rootId, String titleString) {
        Tab tab = mTabModelSelector.getTabById(rootId);
        boolean incognito = tab.isIncognito();

        TitleBitmapFactory titleBitmapFactory =
                incognito ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        Title title = mGroupTitles.get(rootId);
        if (title == null) {
            title = new Title();
            mGroupTitles.put(rootId, title);
            title.register();
        }

        Bitmap titleBitmap = titleBitmapFactory.getTitleBitmap(mContext, titleString);
        for (GroupTitleObserver observer : mGroupTitleObservers) {
            observer.onGroupTitleUpdated(incognito, rootId, titleString, titleBitmap.getWidth());
        }
        title.set(titleBitmap);

        if (mNativeLayerTitleCache != 0) {
            String tabTitle = tab.getTitle();
            boolean isRtl =
                    tabTitle != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(tabTitle)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateGroupLayer(
                            mNativeLayerTitleCache,
                            LayerTitleCache.this,
                            rootId,
                            title.getTitleResId(),
                            incognito,
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
        mFaviconHelper.getLocalFaviconImageForURL(
                tab.getProfile(), tab.getUrl(), mFaviconSize, callback);
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
        FaviconTitle title = mTabTitles.get(tabId);
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

    public void removeTabTitle(int tabId) {
        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) return;
        title.unregister();
        mTabTitles.remove(tabId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateLayer(
                        mNativeLayerTitleCache, LayerTitleCache.this, tabId, -1, -1, false, false);
    }

    public void removeGroupTitle(int rootId) {
        // TODO(crbug.com/326492787): Currently unused. Call to release bitmaps when we actually
        // observe tab group changes (i.e. call this when a tab group is destroyed).
        Title title = mGroupTitles.get(rootId);
        if (title == null) return;
        title.unregister();
        mGroupTitles.remove(rootId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateGroupLayer(
                        mNativeLayerTitleCache, LayerTitleCache.this, rootId, -1, false, false);
    }

    /**
     * Adds an observer.
     *
     * @param observer The observer to add.
     */
    public void addObserver(@NonNull GroupTitleObserver observer) {
        mGroupTitleObservers.addObserver(observer);
    }

    /**
     * Removes an observer.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(@NonNull GroupTitleObserver observer) {
        mGroupTitleObservers.removeObserver(observer);
    }

    private class Title {
        final BitmapDynamicResource mTitle = new BitmapDynamicResource(sNextResourceId++);

        public Title() {}

        public void set(Bitmap titleBitmap) {
            mTitle.setBitmap(titleBitmap);
        }

        public void register() {
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mTitle.getResId(), mTitle);
        }

        public void unregister() {
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mTitle.getResId());
        }

        public int getTitleResId() {
            return mTitle.getResId();
        }
    }

    private class FaviconTitle extends Title {
        private final BitmapDynamicResource mFavicon = new BitmapDynamicResource(sNextResourceId++);

        // We don't want to override updated favicon (e.g. from Tab#onFaviconAvailable) with one
        // fetched from history. You can set this to true / false to control that.
        private boolean mExpectUpdateFromHistory;

        public FaviconTitle() {}

        public void set(Bitmap titleBitmap, Bitmap faviconBitmap, boolean expectUpdateFromHistory) {
            set(titleBitmap);
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = expectUpdateFromHistory;
        }

        public boolean updateFaviconFromHistory(Bitmap faviconBitmap) {
            if (!mExpectUpdateFromHistory) return false;
            mFavicon.setBitmap(faviconBitmap);
            mExpectUpdateFromHistory = false;
            return true;
        }

        @Override
        public void register() {
            super.register();
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mFavicon.getResId(), mFavicon);
        }

        @Override
        public void unregister() {
            super.unregister();
            if (mResourceManager == null) return;
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mFavicon.getResId());
        }

        public int getFaviconResId() {
            return mFavicon.getResId();
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

        void updateGroupLayer(
                long nativeLayerTitleCache,
                LayerTitleCache caller,
                int groupRootId,
                int titleResId,
                boolean isIncognito,
                boolean isRtl);

        void updateFavicon(
                long nativeLayerTitleCache, LayerTitleCache caller, int tabId, int faviconResId);
    }
}
