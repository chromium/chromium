// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator of the single tab switcher in the new tab page on tablet. */
public class SingleTabSwitcherOnTabletMediator implements ConfigurationChangedObserver {
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final int mMarginDefaut;
    private final int mMarginSmallPortrait;
    private Resources mResources;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private Tab mMostRecentTab;
    private boolean mInitialized;
    private boolean mIsScrollableMvtEnabled;
    private boolean mIsMultiFeedEnabled;

    private Runnable mSingleTabCardClickedCallback;

    SingleTabSwitcherOnTabletMediator(PropertyModel propertyModel, Resources resources,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            TabModelSelector tabModelSelector, TabListFaviconProvider tabListFaviconProvider,
            Tab mostRecentTab, boolean isMultiColumnFeedEnabled, boolean isScrollableMvtEnabled,
            Runnable singleTabCardClickedCallback) {
        mPropertyModel = propertyModel;
        mResources = resources;
        mTabListFaviconProvider = tabListFaviconProvider;
        mMostRecentTab = mostRecentTab;
        mIsMultiFeedEnabled = isMultiColumnFeedEnabled;
        mIsScrollableMvtEnabled = isScrollableMvtEnabled;
        mSingleTabCardClickedCallback = singleTabCardClickedCallback;

        if (mIsMultiFeedEnabled) {
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

        mPropertyModel.set(CLICK_LISTENER, v -> {
            TabModel currentTabModel = tabModelSelector.getModel(false);
            TabModelUtils.setIndex(currentTabModel,
                    TabModelUtils.getTabIndexById(currentTabModel, mMostRecentTab.getId()), false);
            if (mSingleTabCardClickedCallback != null) {
                mSingleTabCardClickedCallback.run();
                mSingleTabCardClickedCallback = null;
            }
        });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // The margin doesn't change when 2 row MV tiles are shown.
        if (mIsScrollableMvtEnabled && mIsMultiFeedEnabled) {
            updateMargins(newConfig.orientation);
        }
    }

    void updateMargins(int orientation) {
        if (!mIsMultiFeedEnabled) return;

        int lateralMargin =
                mIsScrollableMvtEnabled && orientation == Configuration.ORIENTATION_PORTRAIT
                ? mMarginSmallPortrait
                : mMarginDefaut;
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
        }

        mPropertyModel.set(IS_VISIBLE, true);
        if (mResources != null) {
            updateMargins(mResources.getConfiguration().orientation);
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
    }

    /**
     * Update the favicon of the single tab switcher.
     */
    private void updateFavicon() {
        assert mTabListFaviconProvider.isInitialized();
        mTabListFaviconProvider.getFaviconDrawableForUrlAsync(mMostRecentTab.getUrl(), false,
                (Drawable favicon) -> { mPropertyModel.set(FAVICON, favicon); });
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
                    tab.removeObserver(this);
                }
            };
            mMostRecentTab.addObserver(tabObserver);
        } else {
            mPropertyModel.set(TITLE, mMostRecentTab.getTitle());
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
    }

    int getMarginDefaultForTesting() {
        return mMarginDefaut;
    }

    int getMarginSmallPortraitForTesting() {
        return mMarginSmallPortrait;
    }
}
