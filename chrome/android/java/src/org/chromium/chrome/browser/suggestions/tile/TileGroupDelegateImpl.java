// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.query_tiles.QueryTileUtils;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Reusable implementation of {@link TileGroup.Delegate}. Performs work in parts of the system that
 * the {@link TileGroup} should not know about.
 */
public class TileGroupDelegateImpl implements TileGroup.Delegate {
    private static final Set<Integer> MVTilesClickForUserAction = new HashSet<>(
            Arrays.asList(WindowOpenDisposition.CURRENT_TAB, WindowOpenDisposition.OFF_THE_RECORD));

    private final Context mContext;
    private final SnackbarManager mSnackbarManager;
    private final SuggestionsNavigationDelegate mNavigationDelegate;
    private final MostVisitedSites mMostVisitedSites;

    private boolean mIsDestroyed;
    private SnackbarController mTileRemovedSnackbarController;
    @BrowserUiUtils.HostSurface
    private int mHostSurface;

    public TileGroupDelegateImpl(Context context, Profile profile,
            SuggestionsNavigationDelegate navigationDelegate, SnackbarManager snackbarManager,
            @BrowserUiUtils.HostSurface int hostSurface) {
        mContext = context;
        mSnackbarManager = snackbarManager;
        mNavigationDelegate = navigationDelegate;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(profile);
        mHostSurface = hostSurface;
    }

    @Override
    public void removeMostVisitedItem(Tile item, Callback<GURL> removalUndoneCallback) {
        assert !mIsDestroyed;

        mMostVisitedSites.addBlocklistedUrl(item.getUrl());
        showTileRemovedSnackbar(item.getUrl(), removalUndoneCallback);
    }

    @Override
    public void openMostVisitedItem(int windowDisposition, Tile item) {
        assert !mIsDestroyed;

        recordClickMvTiles(windowDisposition, mHostSurface);

        String url = item.getUrl().getSpec();

        // TODO(treib): Should we call recordOpenedMostVisitedItem here?
        if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
            recordOpenedTile(item);
        }

        mNavigationDelegate.navigateToSuggestionUrl(windowDisposition, url, false);
        QueryTileUtils.onMostVisitedTileClicked();
    }

    @Override
    public void openMostVisitedItemInGroup(int windowDisposition, Tile item) {
        assert !mIsDestroyed;

        recordClickMvTiles(windowDisposition, mHostSurface);

        String url = item.getUrl().getSpec();

        recordOpenedTile(item);

        mNavigationDelegate.navigateToSuggestionUrl(windowDisposition, url, true);
        QueryTileUtils.onMostVisitedTileClicked();
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

    private void showTileRemovedSnackbar(GURL url, final Callback<GURL> removalUndoneCallback) {
        if (mTileRemovedSnackbarController == null) {
            mTileRemovedSnackbarController = new SnackbarController() {
                @Override
                public void onDismissNoAction(Object actionData) {}

                /** Undoes the tile removal. */
                @Override
                public void onAction(Object actionData) {
                    if (mIsDestroyed) return;
                    GURL url = (GURL) actionData;
                    removalUndoneCallback.onResult(url);
                    mMostVisitedSites.removeBlocklistedUrl(url);
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
        ReturnToChromeUtil.onMVTileOpened();
        mMostVisitedSites.recordOpenedMostVisitedItem(tile);
    }

    /**
     * Records user clicking on MV tiles in New tab page or Start surface.
     * @param windowDisposition How to open (new window, current tab, etc).
     * @param hostSurface The corresponding item of the host name in {@link
     *                    BrowserUiUtils.HostSurface} which indicates the page
     *                    where the Mv tiles located.
     */
    private void recordClickMvTiles(
            int windowDisposition, @BrowserUiUtils.HostSurface int hostSurface) {
        if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
            BrowserUiUtils.recordModuleClickHistogram(
                    hostSurface, BrowserUiUtils.ModuleTypeOnStartAndNTP.MOST_VISITED_TILES);
        }
        if (MVTilesClickForUserAction.contains(windowDisposition)) {
            RecordUserAction.record(
                    "Suggestions.Tile.Tapped." + BrowserUiUtils.getHostName(hostSurface));
        }
    }
}
