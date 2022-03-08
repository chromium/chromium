// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.LEFT_RIGHT_MARGINS;

import android.content.res.Configuration;
import android.content.res.Resources;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.io.IOException;
import java.util.List;

/**
 *  Mediator for handling {@link MvTilesLayout}-related logic.
 */
public class MostVisitedListMediator implements TileGroup.Observer {
    private static final String TAG = "TopSites";

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    private static final int MAX_RESULTS = 12;

    private final Resources mResources;
    private final MvTilesLayout mMvTilesLayout;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private final int mTileViewLandscapePadding;
    private final int mTileViewPortraitEdgePadding;
    private int mTileViewPortraitIntervalPadding;

    private TileRenderer mRenderer;
    private TileGroup mTileGroup;
    private boolean mInitializationComplete;

    public MostVisitedListMediator(Resources resources, MvTilesLayout mvTilesLayout,
            TileRenderer renderer, PropertyModel propertyModel,
            boolean shouldShowPlaceholderPreNative, int parentViewLeftAndRightPaddings,
            boolean isTablet) {
        mResources = resources;
        mMvTilesLayout = mvTilesLayout;
        mRenderer = renderer;
        mModel = propertyModel;
        mIsTablet = isTablet;

        mTileViewLandscapePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape);
        mTileViewPortraitEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);

        maybeSetPortraitIntervalPaddings();

        // Let mv_tiles_container attached to the edge of the screen on phones.
        mModel.set(LEFT_RIGHT_MARGINS, isTablet ? 0 : -parentViewLeftAndRightPaddings);

        if (shouldShowPlaceholderPreNative) maybeShowMvTilesPlaceholder();
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
        mInitializationComplete = true;
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
    public void onTileCountChanged() {}

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
        if (mTileGroup != null) {
            mTileGroup.destroy();
            mTileGroup = null;
        }
        // When mMvTilesLayout is destroyed, all child views are removed. When Start surface is
        // shown again, the child views are added back with initial margins and paddings, but the
        // values of property key were already set before and the settings of margins and paddings
        // values would be skipped. So we need to reset property keys here too.
        mModel.set(EDGE_PADDINGS, 0);
        mModel.set(INTERVAL_PADDINGS, 0);
    }

    public boolean isMVTilesCleanedUp() {
        return mTileGroup == null;
    }

    public void onSwitchToForeground() {
        mTileGroup.onSwitchToForeground(/* trackLoadTask = */ false);
    }

    /**
     * Maybe render MV tiles placeholder pre-native.
     */
    private void maybeShowMvTilesPlaceholder() {
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
}
