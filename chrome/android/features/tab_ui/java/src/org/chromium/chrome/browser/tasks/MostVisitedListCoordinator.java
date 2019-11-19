// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.view.ContextMenu;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.GlobalDiscardableReferencePool;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroup.TileInteractionDelegate;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.TileRenderer;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
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
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TileGroup mTileGroup;
    private TileRenderer mRenderer;
    private ViewGroup mParent;

    public MostVisitedListCoordinator(
            ChromeActivity activity, ViewGroup parent, PropertyModel propertyModel) {
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, parent, MostVisitedListViewBinder::bind);

        mParent = parent;
        Profile profile = Profile.getLastUsedProfile();
        SuggestionsSource suggestionsSource =
                SuggestionsDependencyFactory.getInstance().createSuggestionSource(profile);
        SuggestionsEventReporter eventReporter =
                SuggestionsDependencyFactory.getInstance().createEventReporter();

        DiscardableReferencePool referencePool = GlobalDiscardableReferencePool.getReferencePool();
        ImageFetcher imageFetcher = new ImageFetcher(suggestionsSource, profile, referencePool);
        SnackbarManager snackbarManager = activity.getSnackbarManager();

        mRenderer = new TileRenderer(
                activity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, imageFetcher);

        OfflinePageBridge offlinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);

        TileGroupDelegateImpl tileGroupDelegate =
                new TileGroupDelegateImpl(activity, profile, null, snackbarManager);
        SuggestionsUiDelegate suggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(
                suggestionsSource, eventReporter, profile, referencePool, snackbarManager);
        mTileGroup = new TileGroup(
                mRenderer, suggestionsUiDelegate, null, tileGroupDelegate, this, offlinePageBridge);
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
        return new MostVisitedTileInteractionDelegate(tile);
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

        public MostVisitedTileInteractionDelegate(Tile tile) {
            mTile = tile;
        }

        @Override
        public void setOnClickRunnable(Runnable clickRunnable) {}

        @Override
        public void onClick(View v) {
            ReturnToChromeExperimentsUtil.willHandleLoadUrlFromStartSurface(
                    mTile.getUrl(), PageTransition.AUTO_BOOKMARK);
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            // TODO(mattsimmons): Handle this, likely not a blocker for MVP.
        }
    }

    /** Suggestions UI Delegate for constructing the TileGroup. */
    private static class MostVisitedSuggestionsUiDelegate extends SuggestionsUiDelegateImpl {
        public MostVisitedSuggestionsUiDelegate(SuggestionsSource suggestionsSource,
                SuggestionsEventReporter eventReporter, Profile profile,
                DiscardableReferencePool referencePool, SnackbarManager snackbarManager) {
            super(suggestionsSource, eventReporter, null, profile, null, referencePool,
                    snackbarManager);
        }

        @Override
        public boolean isVisible() {
            return false;
        }
    }
}
