// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.tile.TileGroup.PendingChanges;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
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
    private static final Set<Integer> sMvtClickForUserAction =
            new HashSet<>(
                    Arrays.asList(
                            WindowOpenDisposition.CURRENT_TAB,
                            WindowOpenDisposition.OFF_THE_RECORD));

    private final Context mContext;
    private final Profile mProfile;
    private final SnackbarManager mSnackbarManager;
    private final SuggestionsNavigationDelegate mNavigationDelegate;
    private final MostVisitedSites mMostVisitedSites;

    private @Nullable ModalDialogManager mModalDialogManager;
    private @Nullable PendingChanges mPendingChanges;

    private boolean mIsDestroyed;
    private SnackbarController mTileRemovedSnackbarController;
    private SnackbarController mTileUnpinnedSnackbarController;

    public TileGroupDelegateImpl(
            Context context,
            Profile profile,
            SuggestionsNavigationDelegate navigationDelegate,
            SnackbarManager snackbarManager) {
        mContext = context;
        mProfile = profile;
        mNavigationDelegate = navigationDelegate;
        mSnackbarManager = snackbarManager;
        mMostVisitedSites =
                SuggestionsDependencyFactory.getInstance().createMostVisitedSites(profile);
    }

    // CustomLinkOperations -> TileGroup.Delegate implementation.
    @Override
    public boolean addCustomLink(String name, @Nullable GURL url, @Nullable Integer pos) {
        assert !mIsDestroyed;
        dismissAllSnackbars();
        return mMostVisitedSites.addCustomLink(name, url, pos);
    }

    @Override
    public boolean assignCustomLink(GURL keyUrl, String name, @Nullable GURL url) {
        assert !mIsDestroyed;
        dismissAllSnackbars();
        return mMostVisitedSites.assignCustomLink(keyUrl, name, url);
    }

    @Override
    public boolean deleteCustomLink(GURL keyUrl) {
        assert !mIsDestroyed;
        dismissAllSnackbars();
        return mMostVisitedSites.deleteCustomLink(keyUrl);
    }

    @Override
    public boolean hasCustomLink(GURL keyUrl) {
        assert !mIsDestroyed;
        return mMostVisitedSites.hasCustomLink(keyUrl);
    }

    @Override
    public boolean reorderCustomLink(GURL keyUrl, int newPos) {
        assert !mIsDestroyed;
        dismissAllSnackbars();
        return mMostVisitedSites.reorderCustomLink(keyUrl, newPos);
    }

    // TileGroup.Delegate implementation.
    @Override
    @Initializer
    public void setPendingChanges(PendingChanges pendingChanges) {
        mPendingChanges = pendingChanges;
    }

    @Override
    public void removeMostVisitedItem(Tile item) {
        assert !mIsDestroyed;

        GURL url = item.getUrl();
        // Handle change detection. This only tracks the most recent removal, and not all removals.
        // But if the removal is committed, this is good enough for change detection.
        if (mPendingChanges != null) {
            mPendingChanges.removalUrl = url;
        }
        mMostVisitedSites.addBlocklistedUrl(url);
        showTileRemovedSnackbar(url);
    }

    @Override
    public void openMostVisitedItem(int windowDisposition, Tile item) {
        assert !mIsDestroyed;

        recordClickMvTiles(windowDisposition);

        GURL url = item.getUrl();

        if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
            recordOpenedTile(item);
        }

        if (ChromeFeatureList.sMostVisitedTilesReselect.isEnabled() && tileIsReselectable(item)) {
            if (mNavigationDelegate.maybeSelectTabWithUrl(url)) {
                return;
            }
            // Failed to select existing tab with the same URL, so just navigate.
        }

        mNavigationDelegate.navigateToSuggestionUrl(windowDisposition, url.getSpec(), false);
    }

    @Override
    public void openMostVisitedItemInGroup(int windowDisposition, Tile item) {
        assert !mIsDestroyed;

        recordClickMvTiles(windowDisposition);

        String url = item.getUrl().getSpec();

        recordOpenedTile(item);

        mNavigationDelegate.navigateToSuggestionUrl(windowDisposition, url, true);
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
    }

    @Override
    public void initAndroidPrerenderManager(AndroidPrerenderManager androidPrerenderManager) {
        if (mNavigationDelegate != null) {
            mNavigationDelegate.initAndroidPrerenderManager(androidPrerenderManager);
        }
    }

    @Override
    public CustomTileEditCoordinator createCustomTileEditCoordinator(@Nullable Tile originalTile) {
        assert !mIsDestroyed;

        if (mModalDialogManager == null) {
            mModalDialogManager =
                    new ModalDialogManager(new AppModalPresenter(mContext), ModalDialogType.APP);
        }
        return CustomTileEditCoordinator.make(mModalDialogManager, mContext, originalTile);
    }

    @Override
    public void showTileUnpinSnackbar(Runnable undoHandler) {
        if (mTileUnpinnedSnackbarController == null) {
            mTileUnpinnedSnackbarController =
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(Object actionData) {}

                        /** Undoes the tile removal. */
                        @Override
                        public void onAction(Object actionData) {
                            if (mIsDestroyed) return;
                            Runnable undoHandlerFromData = (Runnable) actionData;
                            undoHandlerFromData.run();
                            RecordUserAction.record("Suggestions.SnackBar.UndoUnpinItem");
                        }
                    };
        }
        Snackbar snackbar =
                Snackbar.make(
                                mContext.getString(R.string.most_visited_item_removed),
                                mTileUnpinnedSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_NTP_MOST_VISITED_UNPIN_UNDO)
                        .setAction(mContext.getString(R.string.undo), undoHandler);
        mSnackbarManager.showSnackbar(snackbar);
    }

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        mIsDestroyed = true;

        dismissAllSnackbars();
        mMostVisitedSites.destroy();
    }

    private void dismissAllSnackbars() {
        if (mTileUnpinnedSnackbarController != null) {
            mSnackbarManager.dismissSnackbars(mTileUnpinnedSnackbarController);
        }
        if (mTileRemovedSnackbarController != null) {
            mSnackbarManager.dismissSnackbars(mTileRemovedSnackbarController);
        }
    }

    private void showTileRemovedSnackbar(GURL url) {
        if (mTileRemovedSnackbarController == null) {
            mTileRemovedSnackbarController =
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(Object actionData) {}

                        /** Undoes the tile removal. */
                        @Override
                        public void onAction(Object actionData) {
                            if (mIsDestroyed) return;
                            GURL url = (GURL) actionData;
                            if (mPendingChanges != null) {
                                mPendingChanges.insertionUrl = url;
                            }
                            mMostVisitedSites.removeBlocklistedUrl(url);
                        }
                    };
        }
        Snackbar snackbar =
                Snackbar.make(
                                mContext.getString(R.string.most_visited_item_removed),
                                mTileRemovedSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_NTP_MOST_VISITED_DELETE_UNDO)
                        .setAction(mContext.getString(R.string.undo), url);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void recordOpenedTile(Tile tile) {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_MOST_VISITED_TILE);
        RecordUserAction.record("MobileNTPMostVisited");
        mMostVisitedSites.recordOpenedMostVisitedItem(tile);
    }

    /**
     * Records user clicking on MV tiles in New tab page.
     *
     * @param windowDisposition How to open (new window, current tab, etc).
     */
    private void recordClickMvTiles(int windowDisposition) {
        if (windowDisposition != WindowOpenDisposition.NEW_WINDOW) {
            BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.MOST_VISITED_TILES);
        }
        if (sMvtClickForUserAction.contains(windowDisposition)) {
            RecordUserAction.record("Suggestions.Tile.Tapped.NewTabPage");
        }
    }

    private boolean tileIsReselectable(Tile tile) {
        // Search suggestions should not reselect existing tab; a new search is always conducted to
        // ensure freshness.
        return !TileUtils.isSearchTile(mProfile, tile);
    }
}
