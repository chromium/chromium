// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator of the single tab switcher in the new tab page on tablet. */
public class SingleTabSwitcherOnTabletMediator {
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private Tab mMostRecentTab;
    private boolean mInitialized;

    SingleTabSwitcherOnTabletMediator(PropertyModel propertyModel,
            TabModelSelector tabModelSelector, TabListFaviconProvider tabListFaviconProvider,
            Tab mostRecentTab) {
        mPropertyModel = propertyModel;
        mTabListFaviconProvider = tabListFaviconProvider;
        mMostRecentTab = mostRecentTab;

        if (mMostRecentTab != null) {
            mPropertyModel.set(CLICK_LISTENER, v -> {
                TabModel currentTabModel = tabModelSelector.getModel(false);
                TabModelUtils.setIndex(currentTabModel,
                        TabModelUtils.getTabIndexById(currentTabModel, mMostRecentTab.getId()),
                        false);
            });
        }
    }

    /**
     * Set the visibility of the single tab card of the {@link NewTabPageLayout} on tablet.
     * @param isVisible Whether the single tab card is visible.
     */
    void setVisibility(boolean isVisible) {
        if (!isVisible || mMostRecentTab == null) {
            mPropertyModel.set(IS_VISIBLE, false);
            return;
        }
        if (!mInitialized) {
            mInitialized = true;
            updateTitle();
            updateFavicon();
        }
        mPropertyModel.set(IS_VISIBLE, true);
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
}