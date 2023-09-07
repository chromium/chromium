// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_NTP_AS_HOME_SURFACE_ENABLED;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_SURFACE_POLISH_ENABLED;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.PLACEHOLDER_VIEW;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.UPDATE_INTERVAL_PADDINGS_TABLET;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.List;

/**
 *  Mediator for handling {@link MostVisitedTilesCarouselLayout} when {@link
 * org.chromium.chrome.browser.flags.ChromeFeatureList#SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID} is
 * enabled or {@link MostVisitedTilesGridLayout} when the feature is disabled -related logic.
 */
public class MostVisitedTilesMediator implements TileGroup.Observer, TemplateUrlServiceObserver {
    private static final String TAG = "TopSites";

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    static final int MAX_RESULTS = 12;

    private final Resources mResources;
    private final UiConfig mUiConfig;
    private final ViewGroup mMvTilesLayout;
    private final ViewStub mNoMvPlaceholderStub;
    private final PropertyModel mModel;
    private final boolean mIsScrollableMVTEnabled;
    private final boolean mIsTablet;
    private final boolean mIsNtpAsHomeSurfaceEnabled;
    private final boolean mIsSurfacePolishEnabled;
    private final int mTileViewLandscapePadding;
    private final int mTileViewPortraitEdgePadding;
    private final Runnable mSnapshotTileGridChangedRunnable;
    private final Runnable mTileCountChangedRunnable;
    private int mTileViewPortraitIntervalPadding;

    private TileRenderer mRenderer;
    private TileGroup mTileGroup;
    private boolean mInitializationComplete;
    private boolean mSearchProviderHasLogo = true;
    private TemplateUrlService mTemplateUrlService;

    private int mTileCarouselLayoutLateralMarginSumForPolish;
    private final int mTileViewEdgePaddingForTabletPolish;
    private int mTileViewIntervalPaddingForTabletPolish;

    public MostVisitedTilesMediator(Resources resources, UiConfig uiConfig, ViewGroup mvTilesLayout,
            ViewStub noMvPlaceholderStub, TileRenderer renderer, PropertyModel propertyModel,
            boolean shouldShowSkeletonUIPreNative, boolean isScrollableMVTEnabled, boolean isTablet,
            @Nullable Runnable snapshotTileGridChangedRunnable,
            @Nullable Runnable tileCountChangedRunnable, boolean isNtpAsHomeSurfaceEnabled) {
        mResources = resources;
        mUiConfig = uiConfig;
        mRenderer = renderer;
        mModel = propertyModel;
        mIsScrollableMVTEnabled = isScrollableMVTEnabled;
        mIsTablet = isTablet;
        mSnapshotTileGridChangedRunnable = snapshotTileGridChangedRunnable;
        mTileCountChangedRunnable = tileCountChangedRunnable;
        mMvTilesLayout = mvTilesLayout;
        mNoMvPlaceholderStub = noMvPlaceholderStub;
        mIsSurfacePolishEnabled = ChromeFeatureList.sSurfacePolish.isEnabled();

        mTileViewLandscapePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape);
        mTileViewPortraitEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        mTileCarouselLayoutLateralMarginSumForPolish =
                mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin_polish) * 2;

        mTileViewEdgePaddingForTabletPolish =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet_polish);
        mTileViewIntervalPaddingForTabletPolish =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet_polish);

        maybeSetPortraitIntervalPaddingsForCarousel();

        if (shouldShowSkeletonUIPreNative) maybeShowMvTilesPreNative();

        mIsNtpAsHomeSurfaceEnabled = isNtpAsHomeSurfaceEnabled;
        mModel.set(IS_NTP_AS_HOME_SURFACE_ENABLED, mIsNtpAsHomeSurfaceEnabled);
        if (mIsScrollableMVTEnabled) {
            mModel.set(IS_SURFACE_POLISH_ENABLED, mIsSurfacePolishEnabled);
        }
    }

    /**
     * Called to initialize this mediator when native is ready.
     */
    public void initWithNative(SuggestionsUiDelegate suggestionsUiDelegate,
            ContextMenuManager contextMenuManager, TileGroup.Delegate tileGroupDelegate,
            OfflinePageBridge offlinePageBridge, TileRenderer renderer) {
        mRenderer = renderer;
        mTileGroup = new TileGroup(renderer, suggestionsUiDelegate, contextMenuManager,
                tileGroupDelegate, /*observer=*/this, offlinePageBridge);
        mTileGroup.startObserving(MAX_RESULTS);

        mTemplateUrlService =
                TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
        mTemplateUrlService.addObserver(this);

        onSearchEngineHasLogoChanged();

        mInitializationComplete = true;
    }

    // TemplateUrlServiceObserver overrides
    @Override
    public void onTemplateURLServiceChanged() {
        onSearchEngineHasLogoChanged();
    }

    /* TileGroup.Observer implementation. */
    @Override
    public void onTileDataChanged() {
        if (mTileGroup.getTileSections().size() < 1) return;

        mRenderer.renderTileSection(mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED),
                mMvTilesLayout, mTileGroup.getTileSetupDelegate());
        mTileGroup.notifyTilesRendered();
        updateTilesViewForCarouselLayout();

        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
        MostVisitedSitesMetadataUtils.getInstance().saveSuggestionListsToFile(
                mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED));
    }

    @Override
    public void onTileCountChanged() {
        if (mTileCountChangedRunnable != null) mTileCountChangedRunnable.run();
        updateTilePlaceholderVisibility();

        if (mIsSurfacePolishEnabled) {
            mModel.set(IS_CONTAINER_VISIBLE, !mTileGroup.isEmpty());
        }
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        updateTileIcon(tile);
        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
    }

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {
        updateOfflineBadge(tile);
        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
    }

    public void onConfigurationChanged() {
        maybeSetPortraitIntervalPaddingsForCarousel();
        updateTilesViewForCarouselLayout();
    }

    public void destroy() {
        if (mMvTilesLayout != null && mIsScrollableMVTEnabled) {
            ((MostVisitedTilesCarouselLayout) mMvTilesLayout).destroy();
        }

        if (mTileGroup != null) {
            mTileGroup.destroy();
            mTileGroup = null;
        }
        if (mTemplateUrlService != null) mTemplateUrlService.removeObserver(this);
    }

    public boolean isMVTilesCleanedUp() {
        return mTileGroup == null;
    }

    public void onSwitchToForeground() {
        mTileGroup.onSwitchToForeground(/* trackLoadTask = */ false);
    }

    /**
     * Maybe render MV tiles skeleton icon pre-native.
     */
    private void maybeShowMvTilesPreNative() {
        if (mInitializationComplete) return;
        try {
            List<Tile> tiles =
                    MostVisitedSitesMetadataUtils.restoreFileToSuggestionListsOnUiThread();
            if (tiles == null) return;
            mRenderer.renderTileSection(tiles, mMvTilesLayout, null);
            updateTilesViewForCarouselLayout();
        } catch (IOException e) {
            Log.i(TAG, "No cached MV tiles file.");
        }
    }

    private void updateTileIcon(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) {
            tileView.renderIcon(tile);
        }
    }

    private void updateOfflineBadge(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    private SuggestionsTileView findTileView(SiteSuggestion data) {
        int childCount = mMvTilesLayout.getChildCount();
        for (int i = 0; i < childCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) mMvTilesLayout.getChildAt(i);
            if (data.equals(tileView.getData())) return tileView;
        }
        return null;
    }

    private void maybeSetPortraitIntervalPaddingsForCarousel() {
        // If it's gird layout (mIsScrollableMVTEnabled is false), the paddings are handled in
        // {@link MostVisitedTilesGridLayout}
        if (!mIsScrollableMVTEnabled
                || mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE
                || mTileViewPortraitIntervalPadding != 0) {
            return;
        }
        if (mIsTablet) {
            mTileViewPortraitIntervalPadding = mTileViewPortraitEdgePadding;
        } else {
            boolean isSmallDevice = mUiConfig.getCurrentDisplayStyle().isSmall();
            int screenWidth = mResources.getDisplayMetrics().widthPixels;
            if (mIsSurfacePolishEnabled) {
                screenWidth -= mTileCarouselLayoutLateralMarginSumForPolish;
            }
            int tileViewWidth = mResources.getDimensionPixelOffset(
                    isSmallDevice ? R.dimen.tile_view_width_condensed : R.dimen.tile_view_width);
            // We want to show four and a half tile view to make users know the MV tiles are
            // scrollable. But the padding should be equal to or larger than tile_view_padding,
            // otherwise the titles among tiles would be overlapped.
            mTileViewPortraitIntervalPadding = Integer.max(
                    -mResources.getDimensionPixelOffset(R.dimen.tile_view_padding),
                    (int) ((screenWidth - mTileViewPortraitEdgePadding - tileViewWidth * 4.5) / 4));
        }
    }

    private void updateTilesViewForCarouselLayout() {
        // If it's gird layout (mIsScrollableMVTEnabled is false), the paddings are handled in
        // {@link MostVisitedTilesGridLayout}
        if (!mIsScrollableMVTEnabled || mMvTilesLayout.getChildCount() < 1) return;

        if (mIsNtpAsHomeSurfaceEnabled && !mIsSurfacePolishEnabled) {
            mModel.set(HORIZONTAL_EDGE_PADDINGS, 0);
            mModel.set(UPDATE_INTERVAL_PADDINGS_TABLET,
                    mResources.getConfiguration().orientation
                            == Configuration.ORIENTATION_LANDSCAPE);
            return;
        }

        if (mIsNtpAsHomeSurfaceEnabled && mIsSurfacePolishEnabled) {
            mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewEdgePaddingForTabletPolish);
            mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewIntervalPaddingForTabletPolish);
            return;
        }

        if (mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewLandscapePadding);
            mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewLandscapePadding);
            return;
        }

        mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewPortraitEdgePadding);
        mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewPortraitIntervalPadding);
    }

    private void onSearchEngineHasLogoChanged() {
        boolean searchEngineHasLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();
        if (mSearchProviderHasLogo == searchEngineHasLogo) return;

        mSearchProviderHasLogo = searchEngineHasLogo;
        updateTilePlaceholderVisibility();

        // TODO(crbug.com/1329288): Remove this when the Feed position experiment is cleaned up.
        if (!mIsScrollableMVTEnabled) {
            ((MostVisitedTilesGridLayout) mMvTilesLayout)
                    .setSearchProviderHasLogo(mSearchProviderHasLogo);
            ViewUtils.requestLayout(
                    mMvTilesLayout, "MostVisitedTilesMediator.onSearchEngineHasLogoChanged");
        }
    }

    /**
     * Shows the most visited placeholder ("Nothing to see here") if there are no most visited
     * items and there is no search provider logo.
     */
    private void updateTilePlaceholderVisibility() {
        if (mTileGroup == null) return;
        boolean showPlaceholder =
                mTileGroup.hasReceivedData() && mTileGroup.isEmpty() && !mSearchProviderHasLogo;

        if (showPlaceholder && mModel.get(PLACEHOLDER_VIEW) == null) {
            mModel.set(PLACEHOLDER_VIEW, mNoMvPlaceholderStub.inflate());
        }
        mModel.set(IS_MVT_LAYOUT_VISIBLE, !showPlaceholder);
    }
}
