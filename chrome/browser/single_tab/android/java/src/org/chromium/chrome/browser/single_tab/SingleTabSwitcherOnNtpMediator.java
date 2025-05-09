// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.SEE_MORE_LINK_CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.URL;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Size;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.HomeModulesMetricsUtils;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator of the single tab switcher in the new tab page on tablet. */
@NullMarked
public class SingleTabSwitcherOnNtpMediator {
    private static final String HISTOGRAM_SEE_MORE_LINK_CLICKED =
            "MagicStack.Clank.SingleTab.SeeMoreLinkClicked";

    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final int mMarginForPhoneAndNarrowWindowOnTablet;

    // It is only non-null for NTP on tablets.
    private final @Nullable UiConfig mUiConfig;
    private final boolean mIsTablet;

    private boolean mInitialized;
    private @Nullable Tab mMostRecentTab;
    private @Nullable Callback<Integer> mSingleTabCardClickedCallback;
    private @Nullable Runnable mSeeMoreLinkClickedCallback;
    private final @Nullable ThumbnailProvider mThumbnailProvider;
    private @Nullable Size mThumbnailSize;
    private @Nullable DisplayStyleObserver mDisplayStyleObserver;
    private final @Nullable ModuleDelegate mModuleDelegate;

    SingleTabSwitcherOnNtpMediator(
            Context context,
            PropertyModel propertyModel,
            TabModelSelector tabModelSelector,
            TabListFaviconProvider tabListFaviconProvider,
            Tab mostRecentTab,
            @Nullable Callback<Integer> singleTabCardClickedCallback,
            @Nullable Runnable seeMoreLinkClickedCallback,
            TabContentManager tabContentManager,
            @Nullable UiConfig uiConfig,
            boolean isTablet,
            @Nullable ModuleDelegate moduleDelegate) {
        mContext = context;
        mPropertyModel = propertyModel;
        mTabListFaviconProvider = tabListFaviconProvider;
        mMostRecentTab = mostRecentTab;
        mSingleTabCardClickedCallback = singleTabCardClickedCallback;
        mSeeMoreLinkClickedCallback = seeMoreLinkClickedCallback;
        mUiConfig = uiConfig;
        mIsTablet = isTablet;
        mModuleDelegate = moduleDelegate;

        mMarginForPhoneAndNarrowWindowOnTablet =
                mContext.getResources()
                        .getDimensionPixelSize(
                                R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet);

        mThumbnailProvider = getThumbnailProvider(tabContentManager);
        if (mThumbnailProvider != null) {
            mThumbnailSize = getThumbnailSize(mContext);
        }

        mPropertyModel.set(
                CLICK_LISTENER,
                v -> {
                    if (mSingleTabCardClickedCallback != null) {
                        mSingleTabCardClickedCallback.onResult(
                                assumeNonNull(mMostRecentTab).getId());
                        mSingleTabCardClickedCallback = null;
                    }
                });
        mPropertyModel.set(
                SEE_MORE_LINK_CLICK_LISTENER,
                () -> {
                    if (mSeeMoreLinkClickedCallback != null) {
                        mSeeMoreLinkClickedCallback.run();
                        mSeeMoreLinkClickedCallback = null;
                        RecordHistogram.recordBooleanHistogram(
                                HISTOGRAM_SEE_MORE_LINK_CLICKED, true);
                    }
                });

        if (mUiConfig != null) {
            assert mIsTablet;
            mDisplayStyleObserver = this::onDisplayStyleChanged;
            mUiConfig.addObserver(mDisplayStyleObserver);
        }

        mTabListFaviconProvider.initWithNative(
                assumeNonNull(tabModelSelector.getModel(/* incognito= */ false).getProfile()));
    }

    private static @Nullable ThumbnailProvider getThumbnailProvider(
            TabContentManager tabContentManager) {
        if (tabContentManager == null) return null;

        return new TabContentManagerThumbnailProvider(tabContentManager);
    }

    private static Size getThumbnailSize(Context context) {
        int resourceId =
                HomeModulesMetricsUtils.useMagicStack()
                        ? R.dimen.single_tab_module_tab_thumbnail_size_big
                        : R.dimen.single_tab_module_tab_thumbnail_size;
        int size = context.getResources().getDimensionPixelSize(resourceId);
        return new Size(size, size);
    }

    private void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        if (mPropertyModel == null) return;

        updateMargins(newDisplayStyle);
    }

    void updateMargins(@Nullable DisplayStyle newDisplayStyle) {
        int lateralMargin = getDefaultLateralMargin();
        if (newDisplayStyle != null && newDisplayStyle.horizontal < HorizontalDisplayStyle.WIDE) {
            lateralMargin = mMarginForPhoneAndNarrowWindowOnTablet;
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
        if (mModuleDelegate != null) {
            mModuleDelegate.onDataReady(getModuleType(), mPropertyModel);
        }

        updateMargins(mUiConfig != null ? mUiConfig.getCurrentDisplayStyle() : null);
    }

    boolean isVisible() {
        return mPropertyModel.get(IS_VISIBLE);
    }

    /**
     * Update the most recent tab to track in the single tab card.
     *
     * @param tabToTrack The tab to track as the most recent tab.
     * @return Whether has a Tab to track. Returns false if the Tab to track is set as null.
     */
    boolean setTab(@Nullable Tab tabToTrack) {
        if (tabToTrack != null && UrlUtilities.isNtpUrl(tabToTrack.getUrl())) {
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
        if (mPropertyModel != null) {
            mPropertyModel.set(CLICK_LISTENER, null);
            if (mMostRecentTab != null) {
                cleanUp();
            }
        }
        if (mUiConfig != null) {
            mUiConfig.removeObserver(assumeNonNull(mDisplayStyleObserver));
            mDisplayStyleObserver = null;
        }
    }

    /** Update the favicon of the single tab switcher. */
    private void updateFavicon() {
        assert mTabListFaviconProvider.isInitialized();
        mTabListFaviconProvider.getFaviconDrawableForTabAsync(
                assumeNonNull(mMostRecentTab),
                (Drawable favicon) -> {
                    mPropertyModel.set(FAVICON, favicon);
                });
    }

    private void mayUpdateTabThumbnail() {
        if (mThumbnailProvider == null || mThumbnailSize == null || mMostRecentTab == null) {
            return;
        }

        mThumbnailProvider.getTabThumbnailWithCallback(
                mMostRecentTab.getId(),
                mThumbnailSize,
                /* isSelected= */ false,
                (@Nullable Drawable tabThumbnail) -> {
                    mPropertyModel.set(TAB_THUMBNAIL, tabThumbnail);
                });
    }

    /** Update the title of the single tab switcher. */
    @VisibleForTesting
    void updateTitle() {
        assumeNonNull(mMostRecentTab);
        if (mMostRecentTab.isLoading() && TextUtils.isEmpty(mMostRecentTab.getTitle())) {
            TabObserver tabObserver =
                    new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab, GURL url) {
                            super.onPageLoadFinished(tab, url);
                            mPropertyModel.set(TITLE, tab.getTitle());
                            mPropertyModel.set(URL, getDomainUrl(tab.getUrl()));
                            tab.removeObserver(this);
                        }
                    };
            mMostRecentTab.addObserver(tabObserver);
        } else {
            mPropertyModel.set(TITLE, mMostRecentTab.getTitle());
            mPropertyModel.set(URL, getDomainUrl(mMostRecentTab.getUrl()));
        }
    }

    private static String getDomainUrl(GURL url) {
        if (HomeModulesMetricsUtils.useMagicStack()) {
            String domainUrl = UrlUtilities.getDomainAndRegistry(url.getSpec(), false);
            return !TextUtils.isEmpty(domainUrl) ? domainUrl : url.getHost();
        } else {
            return url.getHost();
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
        mPropertyModel.set(URL, null);
        mPropertyModel.set(TAB_THUMBNAIL, null);
    }

    int getDefaultLateralMargin() {
        return mIsTablet ? 0 : mMarginForPhoneAndNarrowWindowOnTablet;
    }

    @ModuleType
    int getModuleType() {
        return ModuleType.SINGLE_TAB;
    }
}
