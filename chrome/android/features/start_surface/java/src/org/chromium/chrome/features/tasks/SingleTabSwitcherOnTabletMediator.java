// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.URL;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Size;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.ThumbnailProvider;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator of the single tab switcher in the new tab page on tablet. */
public class SingleTabSwitcherOnTabletMediator implements ConfigurationChangedObserver {
    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final int mMarginDefaut;
    private final int mMarginSmallPortrait;
    private final int mMarginNarrowWindowOnTablet;

    // It is only non-null for NTP on tablets.
    private @Nullable final UiConfig mUiConfig;
    private Resources mResources;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private Tab mMostRecentTab;
    private boolean mInitialized;
    private boolean mIsScrollableMvtEnabled;

    private Runnable mSingleTabCardClickedCallback;
    private boolean mIsSurfacePolishEnabled;
    private ThumbnailProvider mThumbnailProvider;
    private Size mThumbnailSize;
    private @Nullable DisplayStyleObserver mDisplayStyleObserver;

    SingleTabSwitcherOnTabletMediator(
            Context context,
            PropertyModel propertyModel,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TabModelSelector tabModelSelector,
            TabListFaviconProvider tabListFaviconProvider,
            Tab mostRecentTab,
            boolean isScrollableMvtEnabled,
            Runnable singleTabCardClickedCallback,
            @Nullable TabContentManager tabContentManager,
            @Nullable UiConfig uiConfig) {
        mContext = context;
        mPropertyModel = propertyModel;
        mResources = mContext.getResources();
        mTabListFaviconProvider = tabListFaviconProvider;
        mMostRecentTab = mostRecentTab;
        mIsScrollableMvtEnabled = isScrollableMvtEnabled;
        mSingleTabCardClickedCallback = singleTabCardClickedCallback;
        mIsSurfacePolishEnabled = tabContentManager != null;
        mUiConfig = uiConfig;

        mMarginNarrowWindowOnTablet =
                mResources.getDimensionPixelSize(R.dimen.search_box_lateral_margin_polish);
        if (!mIsSurfacePolishEnabled) {
            mActivityLifecycleDispatcher = activityLifecycleDispatcher;
            mMarginDefaut = mResources.getDimensionPixelSize(
                    R.dimen.single_tab_card_lateral_margin_landscape_tablet);
            mMarginSmallPortrait =
                    mResources.getDimensionPixelSize(R.dimen.tile_grid_layout_bleed) / 2
                    + mResources.getDimensionPixelSize(
                            R.dimen.single_tab_card_lateral_margin_portrait_tablet);

            if (mActivityLifecycleDispatcher != null) {
                mActivityLifecycleDispatcher.register(this);
            }
        } else {
            mMarginDefaut = 0;
            mMarginSmallPortrait = 0;
        }

        mThumbnailProvider = SingleTabSwitcherMediator.getThumbnailProvider(tabContentManager);
        if (mThumbnailProvider != null) {
            mThumbnailSize = SingleTabSwitcherMediator.getThumbnailSize(mContext);
        }

        mPropertyModel.set(CLICK_LISTENER, v -> {
            TabModel currentTabModel = tabModelSelector.getModel(false);
            TabModelUtils.setIndex(currentTabModel,
                    TabModelUtils.getTabIndexById(currentTabModel, mMostRecentTab.getId()), false);
            if (mSingleTabCardClickedCallback != null) {
                mSingleTabCardClickedCallback.run();
                mSingleTabCardClickedCallback = null;
            }
        });

        if (mUiConfig != null) {
            assert mIsSurfacePolishEnabled;
            mDisplayStyleObserver = this::onDisplayStyleChanged;
            mUiConfig.addObserver(mDisplayStyleObserver);
        }
    }

    private void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        if (mPropertyModel == null) return;

        updateMargins(mResources.getConfiguration().orientation, newDisplayStyle);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // The margin doesn't change when 2 row MV tiles are shown.
        if (mIsScrollableMvtEnabled) {
            updateMargins(
                    newConfig.orientation,
                    mUiConfig != null ? mUiConfig.getCurrentDisplayStyle() : null);
        }
    }

    void updateMargins(int orientation, DisplayStyle newDisplayStyle) {
        int lateralMargin =
                mIsScrollableMvtEnabled && orientation == Configuration.ORIENTATION_PORTRAIT
                ? mMarginSmallPortrait
                : mMarginDefaut;
        if (newDisplayStyle != null
                && mIsSurfacePolishEnabled
                && newDisplayStyle.horizontal < HorizontalDisplayStyle.WIDE) {
            lateralMargin = mMarginNarrowWindowOnTablet;
        }
        mPropertyModel.set(LATERAL_MARGIN, lateralMargin);
    }

    /**
     * Set the visibility of the single tab card of the {@link NewTabPageLayout} on tablet.
     * @param isVisible Whether the single tab card is visible.
     */
    void setVisibility(boolean isVisible) {
        if (isVisible == mPropertyModel.get(IS_VISIBLE)) return;

        if (!isVisible || mMostRecentTab == null) {
            mPropertyModel.set(IS_VISIBLE, false);
            cleanUp();
            return;
        }

        if (!mInitialized) {
            mInitialized = true;
            updateTitle();
            updateFavicon();
            mayUpdateTabThumbnail();
        }

        mPropertyModel.set(IS_VISIBLE, true);
        if (mResources != null) {
            updateMargins(
                    mResources.getConfiguration().orientation,
                    mUiConfig != null ? mUiConfig.getCurrentDisplayStyle() : null);
        }
    }

    boolean isVisible() {
        return mPropertyModel.get(IS_VISIBLE);
    }

    /**
     * Update the most recent tab to track in the single tab card.
     * @param tabToTrack The tab to track as the most recent tab.
     * @return Whether has a Tab to track. Returns false if the Tab to track is set as null.
     */
    boolean setTab(Tab tabToTrack) {
        if (tabToTrack != null && UrlUtilities.isNTPUrl(tabToTrack.getUrl())) {
            tabToTrack = null;
        }

        if (mMostRecentTab == tabToTrack) return tabToTrack != null;

        if (tabToTrack == null) {
            cleanUp();
            return false;
        } else {
            mMostRecentTab = tabToTrack;
            updateTitle();
            updateFavicon();
            mayUpdateTabThumbnail();
            return true;
        }
    }

    void destroy() {
        if (mActivityLifecycleDispatcher != null) {
            mActivityLifecycleDispatcher.unregister(this);
            mActivityLifecycleDispatcher = null;
        }

        if (mResources != null) {
            mResources = null;
        }

        if (mPropertyModel != null) {
            mPropertyModel.set(CLICK_LISTENER, null);
            if (mMostRecentTab != null) {
                cleanUp();
            }
        }
        if (mUiConfig != null) {
            mUiConfig.removeObserver(mDisplayStyleObserver);
            mDisplayStyleObserver = null;
        }
    }

    /**
     * Update the favicon of the single tab switcher.
     */
    private void updateFavicon() {
        assert mTabListFaviconProvider.isInitialized();
        mTabListFaviconProvider.getFaviconDrawableForUrlAsync(mMostRecentTab.getUrl(), false,
                (Drawable favicon) -> { mPropertyModel.set(FAVICON, favicon); });
    }

    private void mayUpdateTabThumbnail() {
        if (mThumbnailProvider == null) return;

        mThumbnailProvider.getTabThumbnailWithCallback(
                mMostRecentTab.getId(), mThumbnailSize, (Bitmap tabThumbnail) -> {
                    mPropertyModel.set(TAB_THUMBNAIL, tabThumbnail);
                }, true /* forceUpdate */, true /* writeToCache */, false /* isSelected */);
    }

    /**
     * Update the title of the single tab switcher.
     */
    @VisibleForTesting
    void updateTitle() {
        if (mMostRecentTab.isLoading() && TextUtils.isEmpty(mMostRecentTab.getTitle())) {
            TabObserver tabObserver = new EmptyTabObserver() {
                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    super.onPageLoadFinished(tab, url);
                    mPropertyModel.set(TITLE, tab.getTitle());
                    if (mIsSurfacePolishEnabled) {
                        mPropertyModel.set(URL, tab.getUrl().getHost());
                    }
                    tab.removeObserver(this);
                }
            };
            mMostRecentTab.addObserver(tabObserver);
        } else {
            mPropertyModel.set(TITLE, mMostRecentTab.getTitle());
            if (mIsSurfacePolishEnabled) {
                mPropertyModel.set(URL, mMostRecentTab.getUrl().getHost());
            }
        }
    }

    @VisibleForTesting
    boolean getInitialized() {
        return mInitialized;
    }

    @VisibleForTesting
    void setMostRecentTab(Tab mostRecentTab) {
        mMostRecentTab = mostRecentTab;
    }

    private void cleanUp() {
        mMostRecentTab = null;
        mPropertyModel.set(TITLE, null);
        mPropertyModel.set(FAVICON, null);
        if (mIsSurfacePolishEnabled) {
            mPropertyModel.set(URL, null);
            mPropertyModel.set(TAB_THUMBNAIL, null);
        }
    }

    int getMarginDefaultForTesting() {
        return mMarginDefaut;
    }

    int getMarginSmallPortraitForTesting() {
        return mMarginSmallPortrait;
    }
}
