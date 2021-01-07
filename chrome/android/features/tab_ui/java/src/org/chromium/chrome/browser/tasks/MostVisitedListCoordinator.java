// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.ContextMenu;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroup.TileInteractionDelegate;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.TileRenderer;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}.
 *
 * TODO(mattsimmons): Move logic and view manipulation into the mediator/viewbinder. (and add tests)
 */
class MostVisitedListCoordinator implements TileGroup.Observer, TileGroup.TileSetupDelegate {
    private static final int TITLE_LINES = 1;

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    private static final int MAX_RESULTS = 12;
    private final ChromeActivity mActivity;
    private final ViewGroup mParent;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final Supplier<Tab> mParentTabSupplier;
    private TileGroup mTileGroup;
    private TileRenderer mRenderer;
    private SuggestionsUiDelegate mSuggestionsUiDelegate;

    public MostVisitedListCoordinator(ChromeActivity activity, ViewGroup parent,
            PropertyModel propertyModel, Supplier<Tab> parentTabSupplier) {
        mActivity = activity;
        mParent = parent;
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, mParent, MostVisitedListViewBinder::bind);
        mParentTabSupplier = parentTabSupplier;
    }

    public void initialize() {
        Profile profile = Profile.getLastUsedRegularProfile();
        SnackbarManager snackbarManager = mActivity.getSnackbarManager();
        if (mRenderer == null) {
            // This function is never called in incognito mode.
            ImageFetcher imageFetcher = new ImageFetcher(profile);
            mRenderer = new TileRenderer(
                    mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, imageFetcher);

            mSuggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(profile, snackbarManager);
        }
        OfflinePageBridge offlinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        TileGroupDelegateImpl tileGroupDelegate =
                new TileGroupDelegateImpl(mActivity, profile, null, snackbarManager);
        mTileGroup = new TileGroup(mRenderer, mSuggestionsUiDelegate, null, tileGroupDelegate, this,
                offlinePageBridge);
        mTileGroup.startObserving(MAX_RESULTS);
    }

    private void updateTileIcon(Tile tile) {
        for (int i = 0; i < mParent.getChildCount(); i++) {
            View tileView = mParent.getChildAt(i);

            assert tileView instanceof SuggestionsTileView : "Tiles must be SuggestionsTileView";

            SuggestionsTileView suggestionsTileView = (SuggestionsTileView) tileView;

            if (!suggestionsTileView.getUrl().equals(tile.getUrl())) continue;

            ((SuggestionsTileView) mParent.getChildAt(i)).renderIcon(tile);
        }
    }

    /** TileGroup.Observer implementation. */
    @Override
    public void onTileDataChanged() {
        if (mTileGroup.getTileSections().size() < 1) return;

        mRenderer.renderTileSection(
                mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED), mParent, this);
    }

    @Override
    public void onTileCountChanged() {}

    @Override
    public void onTileIconChanged(Tile tile) {}

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {}

    /** TileSetupDelegate implementation. */
    @Override
    public TileInteractionDelegate createInteractionDelegate(Tile tile) {
        return new MostVisitedTileInteractionDelegate(tile, mParentTabSupplier);
    }

    @Override
    public LargeIconBridge.LargeIconCallback createIconLoadCallback(Tile tile) {
        LargeIconBridge.LargeIconCallback callback =
                (icon, fallbackColor, isFallbackColorDefault, iconType) -> {
            if (tile != null) { // Do nothing if the tile was removed.
                tile.setIconType(iconType);
                if (icon == null) {
                    mRenderer.setTileIconFromColor(tile, fallbackColor, isFallbackColorDefault);
                } else {
                    mRenderer.setTileIconFromBitmap(tile, icon);
                }

                updateTileIcon(tile);
            }
        };

        return callback;
    }

    /** Handle interactions with the Most Visited tiles. */
    private static class MostVisitedTileInteractionDelegate implements TileInteractionDelegate {
        private Tile mTile;
        private Supplier<Tab> mParentTabSupplier;

        public MostVisitedTileInteractionDelegate(Tile tile, Supplier<Tab> parentTabSupplier) {
            mTile = tile;
            mParentTabSupplier = parentTabSupplier;
        }

        @Override
        public void setOnClickRunnable(Runnable clickRunnable) {}

        @Override
        public void onClick(View v) {
            ReturnToChromeExperimentsUtil.willHandleLoadUrlFromStartSurface(
                    mTile.getUrl().getSpec(), PageTransition.AUTO_BOOKMARK, null /*incognito*/,
                    mParentTabSupplier.get());
            SuggestionsMetrics.recordTileTapped();
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            // TODO(mattsimmons): Handle this, likely not a blocker for MVP.
        }
    }

    /** Suggestions UI Delegate for constructing the TileGroup. */
    private static class MostVisitedSuggestionsUiDelegate extends SuggestionsUiDelegateImpl {
        public MostVisitedSuggestionsUiDelegate(Profile profile, SnackbarManager snackbarManager) {
            super(null, profile, null, snackbarManager);
        }

        @Override
        public boolean isVisible() {
            return false;
        }
    }
}
