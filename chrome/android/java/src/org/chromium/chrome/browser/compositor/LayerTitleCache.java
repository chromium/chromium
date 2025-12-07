// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;

import androidx.annotation.ColorInt;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TitleBitmapFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.BitmapDynamicResource;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;

/**
 * A version of the {@link LayerTitleCache} that builds native cc::Layer objects that represent the
 * cached title textures.
 */
@JNINamespace("android")
@NullMarked
public class LayerTitleCache {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;

    private final SparseArray<FaviconTitle> mTabTitles = new SparseArray<>();
    private final Map<Token, Title> mGroupTitles = new HashMap<>();
    private final Map<Token, Integer> mSharedAvatarResIds = new HashMap<>();
    private final HashSet<Integer> mTabBubbles = new HashSet<>();
    private final int mFaviconSize;
    private final int mSharedGroupAvatarPaddingPx;
    private final int mBubbleOuterCircleSize;
    private final int mBubbleInnerCircleSize;
    private final int mBubbleOffset;
    private final @ColorInt int mBubbleFillColor;
    private final @ColorInt int mBubbleBorderColor;

    private long mNativeLayerTitleCache;
    private final ResourceManager mResourceManager;

    private @Nullable FaviconHelper mFaviconHelper;
    private final DefaultFaviconHelper mDefaultFaviconHelper;

    /** Responsible for building titles on light themes or standard tabs. */
    protected final TitleBitmapFactory mStandardTitleBitmapFactory;

    /** Responsible for building incognito or dark theme titles. */
    protected final TitleBitmapFactory mDarkTitleBitmapFactory;

    /**
     * @param context The Android {@link Context}.
     * @param resourceManager The manager for static resources to be used by native layers.
     * @param tabStripHeightPx The height of the tab strip in pixels.
     * @param tabModelSelector The {@link TabModelSelector} to retrieve {@link TabGroupModelFilter}
     *     and get {@link Tab} by id.
     */
    public LayerTitleCache(
            Context context,
            ResourceManager resourceManager,
            int tabStripHeightPx,
            TabModelSelector tabModelSelector) {
        mContext = context;
        mResourceManager = resourceManager;
        mTabModelSelector = tabModelSelector;
        Resources res = context.getResources();
        final int fadeWidthPx = res.getDimensionPixelOffset(R.dimen.border_texture_title_fade);
        final int faviconStartPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_start_padding);
        final int faviconEndPaddingPx =
                res.getDimensionPixelSize(R.dimen.tab_title_favicon_end_padding);
        mSharedGroupAvatarPaddingPx =
                res.getDimensionPixelSize(R.dimen.tablet_shared_group_avatar_padding);
        mFaviconSize = res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_size);
        mStandardTitleBitmapFactory =
                new TitleBitmapFactory(context, /* incognito= */ false, tabStripHeightPx);
        mDarkTitleBitmapFactory =
                new TitleBitmapFactory(context, /* incognito= */ true, tabStripHeightPx);
        mDefaultFaviconHelper = new DefaultFaviconHelper();
        mBubbleOuterCircleSize =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_outer_size);
        mBubbleInnerCircleSize =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_inner_size);
        mBubbleOffset =
                res.getDimensionPixelSize(R.dimen.compositor_tab_title_favicon_bubble_offset);
        mBubbleBorderColor =
                TabUiThemeUtil.getTabStripBackgroundColor(context, /* isIncognito= */ false);
        mBubbleFillColor = TabUiThemeProvider.getTabBubbleFillColor(context);
        mNativeLayerTitleCache =
                LayerTitleCacheJni.get()
                        .init(
                                this,
                                fadeWidthPx,
                                faviconStartPaddingPx,
                                faviconEndPaddingPx,
                                R.drawable.spinner,
                                R.drawable.spinner_white,
                                mBubbleInnerCircleSize,
                                mBubbleOuterCircleSize,
                                mBubbleOffset,
                                mBubbleFillColor,
                                mBubbleBorderColor,
                                mResourceManager);
    }

    /** Destroys the native reference. */
    public void shutDown() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }

        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get().destroy(mNativeLayerTitleCache);
        mNativeLayerTitleCache = 0;
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

    /**
     * @param tabId The ID of the tab that needs to show the notification bubble.
     */
    public void updateTabBubble(int tabId, boolean showBubble) {
        if (showBubble) {
            mTabBubbles.add(tabId);
        } else {
            mTabBubbles.remove(tabId);
        }
        LayerTitleCacheJni.get().updateTabBubble(mNativeLayerTitleCache, tabId, showBubble);
    }

    public String getUpdatedTitle(Tab tab, String defaultTitle) {
        // If content view core is null, tab does not have direct access to the favicon, and we
        // will initially show default favicon. But favicons are stored in the history database, so
        // we will fetch favicons asynchronously from database.
        String titleString = getTitleForTab(tab, defaultTitle);
        Bitmap tabFavicon = TabFavicon.getBitmap(tab);
        getUpdatedTitleInternal(tab, titleString, tabFavicon);
        if (tabFavicon == null) fetchFaviconForTab(tab);
        return titleString;
    }

    private String getUpdatedTitleInternal(
            Tab tab, String titleString, @Nullable Bitmap tabFavicon) {
        final int tabId = tab.getId();
        boolean isDarkTheme = tab.isIncognito();
        boolean fetchFaviconFromHistory = tabFavicon == null;

        if (fetchFaviconFromHistory) {
            tabFavicon = getDefaultFavicon(tab);
        }
        assert tabFavicon != null;

        TitleBitmapFactory titleBitmapFactory =
                isDarkTheme ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        FaviconTitle title = mTabTitles.get(tabId);
        if (title == null) {
            title = new FaviconTitle();
            mTabTitles.put(tabId, title);
            title.register();
        }

        title.set(
                titleBitmapFactory.getTabTitleBitmap(titleString),
                titleBitmapFactory.getFaviconBitmap(tabFavicon),
                fetchFaviconFromHistory);

        boolean showBubble = mTabBubbles.contains(tab.getId());
        if (mNativeLayerTitleCache != 0) {
            String tabTitle = tab.getTitle();
            boolean isRtl =
                    tabTitle != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(tabTitle)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateLayer(
                            mNativeLayerTitleCache,
                            tabId,
                            title.getTitleResId(),
                            title.getFaviconResId(),
                            isDarkTheme,
                            isRtl,
                            showBubble);
        }
        return titleString;
    }

    @CalledByNative
    private void buildUpdatedGroupTitle(Token groupId, boolean incognito) {
        TabGroupModelFilter filter =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(incognito);
        assumeNonNull(filter);
        if (!filter.tabGroupExists(groupId)) return;

        String titleString = TabGroupTitleUtils.getDisplayableTitle(mContext, filter, groupId);
        getUpdatedGroupTitle(groupId, titleString, incognito);
    }

    public @Nullable String getUpdatedGroupTitle(
            Token groupId, @Nullable String titleString, boolean incognito) {
        if (TextUtils.isEmpty(titleString)) return null;

        getUpdatedGroupTitleInternal(groupId, titleString, incognito);
        return titleString;
    }

    private void getUpdatedGroupTitleInternal(
            Token groupId, String titleString, boolean incognito) {
        TitleBitmapFactory titleBitmapFactory =
                incognito ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;

        Title title = mGroupTitles.get(groupId);
        if (title == null) {
            title = new Title();
            mGroupTitles.put(groupId, title);
            title.register();
        }

        TabGroupModelFilter filter =
                mTabModelSelector.getTabGroupModelFilterProvider().getCurrentTabGroupModelFilter();
        assert filter != null;
        Bitmap titleBitmap =
                titleBitmapFactory.getGroupTitleBitmap(filter, mContext, groupId, titleString);
        if (titleBitmap == null) return;
        title.set(titleBitmap);

        Integer avatarResId = mSharedAvatarResIds.get(groupId);
        ViewResourceAdapter avatarResource = null;
        if (avatarResId != null) {
            avatarResource = getResourceAdapterFromLoader(avatarResId);
            if (avatarResource != null) avatarResource.invalidate(null);
        }

        if (mNativeLayerTitleCache != 0) {
            boolean isRtl =
                    titleString != null
                            && LocalizationUtils.getFirstStrongCharacterDirection(titleString)
                                    == LocalizationUtils.RIGHT_TO_LEFT;
            LayerTitleCacheJni.get()
                    .updateGroupLayer(
                            mNativeLayerTitleCache,
                            groupId,
                            title.getTitleResId(),
                            avatarResource == null ? Resources.ID_NULL : avatarResId,
                            avatarResource == null ? 0 : mSharedGroupAvatarPaddingPx,
                            incognito,
                            isRtl);
        }
    }

    /**
     * @param incognito Whether or not the tab group is from the Incognito model.
     * @param titleString The title of the tab group.
     * @return The width in px of the title.
     */
    public int getGroupTitleWidth(boolean incognito, String titleString) {
        if (titleString == null) return 0;

        TitleBitmapFactory titleBitmapFactory =
                incognito ? mDarkTitleBitmapFactory : mStandardTitleBitmapFactory;
        return titleBitmapFactory.getGroupTitleWidth(titleString);
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

        if (tab.getTabGroupId() != null && !tab.isOffTheRecord()) {
            // This mirrors the async tab favicon request implementation for tab list.
            // See TabListFaviconProvider#getFaviconForTabAsync for more detailed notes.
            // TODO(crbug.com/394165786): Unify with the aforementioned TabListFaviconProvider code.
            mFaviconHelper.getForeignFaviconImageForURL(
                    tab.getProfile(), tab.getUrl(), mFaviconSize, callback);
        } else {
            mFaviconHelper.getLocalFaviconImageForURL(
                    tab.getProfile(), tab.getUrl(), mFaviconSize, callback);
        }
    }

    /** Returns a chrome favicon if the tab is a native page. else returns a default favicon. */
    public Bitmap getDefaultFavicon(Tab tab) {
        boolean isDarkTheme = tab.isIncognito();
        return IncognitoUtils.shouldOpenIncognitoAsWindow() && isDarkTheme
                ? mDefaultFaviconHelper.getDefaultFaviconBitmap(
                        mContext,
                        tab.getUrl(),
                        /* useDarkIcon= */ false,
                        /* useIncognitoNtpIcon= */ true)
                : mDefaultFaviconHelper.getDefaultFaviconBitmap(
                        mContext, tab.getUrl(), !isDarkTheme, /* useIncognitoNtpIcon= */ false);
    }

    private @Nullable ViewResourceAdapter getResourceAdapterFromLoader(int resId) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        return (ViewResourceAdapter) dynamicResourceLoader.getResource(resId);
    }

    public void registerSharedGroupAvatar(Token groupId, ViewResourceAdapter avatarResource) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        int resId = View.generateViewId();
        dynamicResourceLoader.registerResource(resId, avatarResource);
        mSharedAvatarResIds.put(groupId, resId);
    }

    private void unregisterSharedGroupAvatar(int resId) {
        DynamicResourceLoader dynamicResourceLoader = mResourceManager.getDynamicResourceLoader();
        dynamicResourceLoader.unregisterResource(resId);
    }

    /**
     * Comes up with a valid title to return for a tab.
     *
     * @param tab The {@link Tab} to build a title for.
     * @return The title to use.
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

        boolean showBubble = mTabBubbles.contains(tab.getId());
        if (!title.updateFaviconFromHistory(faviconBitmap)) return;

        if (mNativeLayerTitleCache != 0) {
            LayerTitleCacheJni.get()
                    .updateIcon(mNativeLayerTitleCache, tabId, title.getFaviconResId(), showBubble);
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
                        mNativeLayerTitleCache,
                        tabId,
                        Resources.ID_NULL,
                        Resources.ID_NULL,
                        false,
                        false,
                        false);
    }

    public void removeGroupTitle(@Nullable Token groupId) {
        Title title = mGroupTitles.get(groupId);
        if (title == null) return;
        title.unregister();
        mGroupTitles.remove(groupId);
        if (mNativeLayerTitleCache == 0) return;
        LayerTitleCacheJni.get()
                .updateGroupLayer(
                        mNativeLayerTitleCache,
                        groupId,
                        Resources.ID_NULL,
                        Resources.ID_NULL,
                        0,
                        false,
                        false);
    }

    public void removeSharedGroupAvatar(Token groupId) {
        Integer resId = mSharedAvatarResIds.get(groupId);
        if (resId == null) return;
        unregisterSharedGroupAvatar(resId);
        mSharedAvatarResIds.remove(groupId);
    }

    private class Title {
        final BitmapDynamicResource mTitle = new BitmapDynamicResource(View.generateViewId());

        public Title() {}

        public void set(@Nullable Bitmap titleBitmap) {
            mTitle.setBitmap(titleBitmap);
        }

        public void register() {
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mTitle.getResId(), mTitle);
        }

        public void unregister() {
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.unregisterResource(mTitle.getResId());
        }

        public int getTitleResId() {
            return mTitle.getResId();
        }
    }

    private class FaviconTitle extends Title {
        private final BitmapDynamicResource mFavicon =
                new BitmapDynamicResource(View.generateViewId());

        // We don't want to override updated favicon (e.g. from Tab#onFaviconAvailable) with one
        // fetched from history. You can set this to true / false to control that.
        private boolean mExpectUpdateFromHistory;

        public FaviconTitle() {}

        public void set(
                @Nullable Bitmap titleBitmap,
                @Nullable Bitmap faviconBitmap,
                boolean expectUpdateFromHistory) {
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
            DynamicResourceLoader loader = mResourceManager.getBitmapDynamicResourceLoader();
            loader.registerResource(mFavicon.getResId(), mFavicon);
        }

        @Override
        public void unregister() {
            super.unregister();
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
                LayerTitleCache self,
                int fadeWidth,
                int faviconStartPadding,
                int faviconEndPadding,
                int spinnerResId,
                int spinnerIncognitoResId,
                int tabBubbleInnerDimension,
                int tabBubbleOuterDimension,
                int bubbleOffset,
                @ColorInt int tabBubbleInnerColor,
                @ColorInt int tabBubbleOuterColor,
                ResourceManager resourceManager);

        void destroy(long nativeLayerTitleCache);

        void updateLayer(
                long nativeLayerTitleCache,
                int tabId,
                int titleResId,
                int faviconResId,
                boolean isIncognito,
                boolean isRtl,
                boolean showBubble);

        void updateGroupLayer(
                long nativeLayerTitleCache,
                @Nullable Token groupId,
                int titleResId,
                int avatarResId,
                int avatarPadding,
                boolean isIncognito,
                boolean isRtl);

        void updateIcon(
                long nativeLayerTitleCache, int tabId, int faviconResId, boolean showBubble);

        void updateTabBubble(long nativeLayerTitleCache, int tabId, boolean showBubble);
    }
}
