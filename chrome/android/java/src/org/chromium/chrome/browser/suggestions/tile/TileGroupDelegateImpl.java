// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/**
 * Reusable implementation of {@link TileGroup.Delegate}. Performs work in parts of the system that
 * the {@link TileGroup} should not know about.
 */
public class TileGroupDelegateImpl implements TileGroup.Delegate {
    private final Context mContext;
    private final SnackbarManager mSnackbarManager;
    private final SuggestionsNavigationDelegate mNavigationDelegate;
    private final MostVisitedSites mMostVisitedSites;

    private boolean mIsDestroyed;
    private SnackbarController mTileRemovedSnackbarController;

    public TileGroupDelegateImpl(ChromeActivity activity, Profile profile,
            SuggestionsNavigationDelegate navigationDelegate, SnackbarManager snackbarManager) {
        mContext = activity;
        mSnackbarManager = snackbarManager;
        mNavigationDelegate = navigationDelegate;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(profile);
    }

    @Override
    public void removeMostVisitedItem(Tile item, Callback<String> removalUndoneCallback) {
        assert !mIsDestroyed;

        mMostVisitedSites.addBlacklistedUrl(item.getUrl());
        showTileRemovedSnackbar(item.getUrl(), removalUndoneCallback);
    }

    @Override
    public void openMostVisitedItem(int windowDisposition, Tile item) {
        assert !mIsDestroyed;

        String url = item.getUrl();

        // TODO(treib): Should we call recordOpenedMostVisitedItem here?
        if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
            recordOpenedTile(item);
        }

        mNavigationDelegate.navigateToSuggestionUrl(windowDisposition, url);
    }

    @Override
    public void setMostVisitedSitesObserver(MostVisitedSites.Observer observer, int maxResults) {
        assert !mIsDestroyed;

        mMostVisitedSites.setObserver(observer, maxResults);
    }

    @Override
    public void onLoadingComplete(List<Tile> tiles) {
        // This method is called after network calls complete. It could happen after the suggestions
        // surface is destroyed.
        if (mIsDestroyed) return;

        for (Tile tile : tiles) {
            mMostVisitedSites.recordTileImpression(tile);
        }

        mMostVisitedSites.recordPageImpression(tiles.size());

        for (Tile tile : tiles) {
            if (tile.isOfflineAvailable()) {
                SuggestionsMetrics.recordTileOfflineAvailability(tile.getIndex());
            }
        }
    }

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        mIsDestroyed = true;

        if (mTileRemovedSnackbarController != null) {
            mSnackbarManager.dismissSnackbars(mTileRemovedSnackbarController);
        }
        mMostVisitedSites.destroy();
    }

    private void showTileRemovedSnackbar(String url, final Callback<String> removalUndoneCallback) {
        if (mTileRemovedSnackbarController == null) {
            mTileRemovedSnackbarController = new SnackbarController() {
                @Override
                public void onDismissNoAction(Object actionData) {}

                /** Undoes the tile removal. */
                @Override
                public void onAction(Object actionData) {
                    if (mIsDestroyed) return;
                    String url = (String) actionData;
                    removalUndoneCallback.onResult(url);
                    mMostVisitedSites.removeBlacklistedUrl(url);
                }
            };
        }
        Snackbar snackbar = Snackbar.make(mContext.getString(R.string.most_visited_item_removed),
                                            mTileRemovedSnackbarController, Snackbar.TYPE_ACTION,
                                            Snackbar.UMA_NTP_MOST_VISITED_DELETE_UNDO)
                                    .setAction(mContext.getString(R.string.undo), url);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void recordOpenedTile(Tile tile) {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_MOST_VISITED_TILE);
        RecordUserAction.record("MobileNTPMostVisited");
        NewTabPageUma.recordExplicitUserNavigation(
                tile.getUrl(), NewTabPageUma.RAPPOR_ACTION_VISITED_SUGGESTED_TILE);
        mMostVisitedSites.recordOpenedMostVisitedItem(tile);
    }
}
