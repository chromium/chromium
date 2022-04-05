// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.PLACEHOLDER_VIEW;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.List;

/**
 *  Mediator for handling {@link MvTilesLayout}-related logic.
 */
public class MostVisitedListMediator implements TileGroup.Observer, TemplateUrlServiceObserver {
    private static final String TAG = "TopSites";

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    private static final int MAX_RESULTS = 12;

    private final Resources mResources;
    private final MvTilesLayout mMvTilesLayout;
    private final ViewStub mNoMvPlaceholderStub;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private final int mTileViewLandscapePadding;
    private final int mTileViewPortraitEdgePadding;
    private int mTileViewPortraitIntervalPadding;

    private TileRenderer mRenderer;
    private TileGroup mTileGroup;
    private boolean mInitializationComplete;

    public MostVisitedListMediator(Resources resources, View mvTilesContainerLayout,
            TileRenderer renderer, PropertyModel propertyModel,
            boolean shouldShowSkeletonUIPreNative, boolean isTablet) {
        mResources = resources;
        mRenderer = renderer;
        mModel = propertyModel;
        mIsTablet = isTablet;
        mMvTilesLayout = mvTilesContainerLayout.findViewById(R.id.mv_tiles_layout);
        mNoMvPlaceholderStub = mvTilesContainerLayout.findViewById(R.id.tile_grid_placeholder_stub);

        mTileViewLandscapePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape);
        mTileViewPortraitEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);

        maybeSetPortraitIntervalPaddings();

        if (shouldShowSkeletonUIPreNative) maybeShowMvTilesPreNative();
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

        updateTileGridPlaceholderVisibility();
        TemplateUrlServiceFactory.get().addObserver(this);

        mInitializationComplete = true;
    }

    // TemplateUrlServiceObserver overrides
    @Override
    public void onTemplateURLServiceChanged() {
        updateTileGridPlaceholderVisibility();
    }

    /* TileGroup.Observer implementation. */
    @Override
    public void onTileDataChanged() {
        if (mTileGroup.getTileSections().size() < 1) return;

        mRenderer.renderTileSection(mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED),
                mMvTilesLayout, mTileGroup.getTileSetupDelegate());
        mTileGroup.notifyTilesRendered();
        updateTilesViewLayout();

        MostVisitedSitesMetadataUtils.getInstance().saveSuggestionListsToFile(
                mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED));
    }

    @Override
    public void onTileCountChanged() {
        updateTileGridPlaceholderVisibility();
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        updateTileIcon(tile);
    }

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {
        updateOfflineBadge(tile);
    }

    public void onConfigurationChanged() {
        maybeSetPortraitIntervalPaddings();
        updateTilesViewLayout();
    }

    public void destroy() {
        if (mMvTilesLayout != null) mMvTilesLayout.destroy();
        if (mTileGroup != null) {
            mTileGroup.destroy();
            mTileGroup = null;
        }
        TemplateUrlServiceFactory.get().removeObserver(this);
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
            updateTilesViewLayout();
        } catch (IOException e) {
            Log.i(TAG, "No cached MV tiles file.");
        }
    }

    private void updateTileIcon(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile);
        if (tileView != null) {
            tileView.renderIcon(tile);
        }
    }

    private void updateOfflineBadge(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile);
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    private SuggestionsTileView findTileView(Tile tile) {
        return mMvTilesLayout.findTileView(tile);
    }

    private void maybeSetPortraitIntervalPaddings() {
        if (mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE
                || mTileViewPortraitIntervalPadding != 0) {
            return;
        }
        if (mIsTablet) {
            mTileViewPortraitIntervalPadding = mTileViewPortraitEdgePadding;
        } else {
            int screenWidth = mResources.getDisplayMetrics().widthPixels;
            int tileViewWidth = mResources.getDimensionPixelOffset(R.dimen.tile_view_width);
            // We want to show four and a half tile view to make users know the MV tiles are
            // scrollable.
            mTileViewPortraitIntervalPadding =
                    (int) ((screenWidth - mTileViewPortraitEdgePadding - tileViewWidth * 4.5) / 4);
        }
    }

    private void updateTilesViewLayout() {
        if (mMvTilesLayout.getChildCount() < 1) return;

        if (mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            mModel.set(EDGE_PADDINGS, mTileViewLandscapePadding);
            mModel.set(INTERVAL_PADDINGS, mTileViewLandscapePadding);
            return;
        }

        mModel.set(EDGE_PADDINGS, mTileViewPortraitEdgePadding);
        mModel.set(INTERVAL_PADDINGS, mTileViewPortraitIntervalPadding);
    }

    /**
     * Shows the most visited placeholder ("Nothing to see here") if there are no most visited
     * items and there is no search provider logo.
     */
    private void updateTileGridPlaceholderVisibility() {
        if (mTileGroup == null) return;
        boolean searchProviderHasLogo =
                TemplateUrlServiceFactory.get().doesDefaultSearchEngineHaveLogo();
        boolean showPlaceholder =
                mTileGroup.hasReceivedData() && mTileGroup.isEmpty() && !searchProviderHasLogo;

        if (showPlaceholder && mModel.get(PLACEHOLDER_VIEW) == null) {
            mModel.set(PLACEHOLDER_VIEW, mNoMvPlaceholderStub.inflate());
        }
        mModel.set(IS_MVT_LAYOUT_VISIBLE, !showPlaceholder);
    }
}
