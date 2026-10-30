// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.SearchEngineService;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.preloading.AndroidPrerenderManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.tile.tile_edit_dialog.CustomTileEditCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
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
@NullMarked
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
    private final boolean mIsAim3pEntrypointEnabled;

    private @Nullable ModalDialogManager mModalDialogManager;
    private boolean mIsDestroyed;
    private @Nullable SnackbarController mTileRemovedSnackbarController;
    private @Nullable SnackbarController mTileUnpinnedSnackbarController;
    private @Nullable NullableObservableSupplier<AiModeButtonUiConfig>
            mAiModeButtonUiConfigSupplier;
    private @Nullable NonNullObservableSupplier<StatusIconResource> mAiModeButtonIconSupplier;
    private @Nullable Callback<@Nullable AiModeButtonUiConfig> mAiModeButtonConfigObserver;
    private @Nullable Callback<StatusIconResource> mAiModeButtonIconObserver;

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
        mIsAim3pEntrypointEnabled = OmniboxFeatures.isAim3pEntrypointEnabled();
        if (mIsAim3pEntrypointEnabled) {
            SearchEngineService searchEngineService = SearchEngineService.getForProfile(profile);
            mAiModeButtonUiConfigSupplier = searchEngineService.getAiModeButtonUiConfigSupplier();
            mAiModeButtonIconSupplier = searchEngineService.getAiModeButtonIconSupplier();
        }
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

    @Override
    public void removeMostVisitedItem(Tile item) {
        assert !mIsDestroyed;

        GURL url = item.getUrl();
        mMostVisitedSites.addBlocklistedUrl(url);
        showTileRemovedSnackbar(() -> mMostVisitedSites.removeBlocklistedUrl(url));
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

        if (mIsAim3pEntrypointEnabled && mAiModeButtonConfigObserver == null) {
            // Native MostVisitedSites does not observe TemplateURLService, so TileGroup is told
            // directly whenever the AI Mode config changes (e.g. on a DSE switch) and re-evaluates
            // whether to show, update, or hide the AI Mode tile from its cached suggestions.
            mAiModeButtonConfigObserver =
                    unused -> {
                        if (mIsDestroyed) return;

                        observer.onAiModeTileConfigChanged();
                    };
            // Notifies TileGroup to refresh the AI Mode tile's icon in-place once the 3P favicon
            // bitmap finishes downloading.
            mAiModeButtonIconObserver =
                    unused -> {
                        if (mIsDestroyed) return;

                        GURL aiModeTileUrl = getAiModeTileUrl();
                        if (!GURL.isEmptyOrInvalid(aiModeTileUrl)) {
                            observer.onIconMadeAvailable(aiModeTileUrl);
                        }
                    };
            assumeNonNull(mAiModeButtonUiConfigSupplier)
                    .addSyncObserver(mAiModeButtonConfigObserver);
            assumeNonNull(mAiModeButtonIconSupplier).addSyncObserver(mAiModeButtonIconObserver);
        }

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
                        public void onDismissNoAction(@Nullable Object actionData) {}

                        /** Undoes the tile removal. */
                        @Override
                        public void onAction(@Nullable Object actionData) {
                            if (mIsDestroyed) return;
                            Runnable undoHandlerFromData = (Runnable) actionData;
                            assumeNonNull(undoHandlerFromData).run();
                            RecordUserAction.record("Suggestions.SnackBar.UndoUnpinItem");
                        }
                    };
        }
        Snackbar snackbar =
                Snackbar.make(
                                mContext.getString(R.string.custom_tile_unpinned),
                                mTileUnpinnedSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_NTP_MOST_VISITED_UNPIN_UNDO)
                        .setAction(mContext.getString(R.string.undo), undoHandler);
        mSnackbarManager.showSnackbar(snackbar);
    }

    @Override
    public double getSuggestionScore(GURL url) {
        return mMostVisitedSites.getSuggestionScore(url);
    }

    @Override
    public void destroy() {
        assert !mIsDestroyed;
        mIsDestroyed = true;

        if (mAiModeButtonConfigObserver != null) {
            assumeNonNull(mAiModeButtonUiConfigSupplier)
                    .removeObserver(mAiModeButtonConfigObserver);
            mAiModeButtonConfigObserver = null;
        }
        if (mAiModeButtonIconObserver != null) {
            assumeNonNull(mAiModeButtonIconSupplier).removeObserver(mAiModeButtonIconObserver);
            mAiModeButtonIconObserver = null;
        }

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

    @Override
    public void showTileRemovedSnackbar(Runnable undoHandler) {
        if (mTileRemovedSnackbarController == null) {
            mTileRemovedSnackbarController =
                    new SnackbarController() {
                        @Override
                        public void onDismissNoAction(@Nullable Object actionData) {}

                        /** Undoes the tile removal. */
                        @Override
                        public void onAction(@Nullable Object actionData) {
                            if (mIsDestroyed) return;

                            Runnable undoHandlerFromData = (Runnable) actionData;
                            assumeNonNull(undoHandlerFromData).run();
                        }
                    };
        }
        Snackbar snackbar =
                Snackbar.make(
                                mContext.getString(R.string.most_visited_item_removed),
                                mTileRemovedSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_NTP_MOST_VISITED_DELETE_UNDO)
                        .setAction(mContext.getString(R.string.undo), undoHandler);
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

    /**
     * Returns the AI Mode tile title from the DSE config when Aim3pEntrypoint is enabled (or null
     * if unavailable, hiding the tile), or the default 1P label when the feature is disabled.
     */
    @Override
    public @Nullable String getAiModeTileTitle() {
        if (mIsAim3pEntrypointEnabled) {
            AiModeButtonUiConfig config = assumeNonNull(mAiModeButtonUiConfigSupplier).get();
            return config != null ? config.text : null;
        }

        return mContext.getString(R.string.ai_mode_entrypoint_label);
    }

    /**
     * Returns the AI Mode navigation URL from the DSE config (falling back to the 1P composeplate
     * URL when empty, or null if no config is available) when Aim3pEntrypoint is enabled, or the 1P
     * composeplate URL when the feature is disabled.
     */
    @Override
    public @Nullable GURL getAiModeTileUrl() {
        // TODO(crbug.com/549681251): Refactors this URL resolution logic to share the
        // implementation with SearchProviderInfoDelegate#getComposeplateUrl() if the MVT AI Mode
        // tile arm launches.
        if (mIsAim3pEntrypointEnabled) {
            AiModeButtonUiConfig config = assumeNonNull(mAiModeButtonUiConfigSupplier).get();
            if (config == null) return null;

            // 3P DSEs provide navigationUrlEmpty in the config; 1P Google leaves it empty and
            // uses getComposeplateUrl().
            return !config.navigationUrlEmpty.isEmpty()
                    ? config.navigationUrlEmpty
                    : getComposeplateUrl();
        }

        return getComposeplateUrl();
    }

    private @Nullable GURL getComposeplateUrl() {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(mProfile);
        return templateUrlService != null ? templateUrlService.getComposeplateUrl() : null;
    }

    /**
     * Returns the AI Mode tile accessibility description from the DSE config when Aim3pEntrypoint
     * is enabled (or null if unavailable), or the default 1P shortcut description when the feature
     * is disabled.
     */
    @Override
    public @Nullable String getAiModeTileContentDescription() {
        if (mIsAim3pEntrypointEnabled) {
            AiModeButtonUiConfig config = assumeNonNull(mAiModeButtonUiConfigSupplier).get();
            return config != null ? config.a11yLabel : null;
        }

        return mContext.getString(R.string.accessibility_ai_mode_mvt_tile);
    }

    /**
     * Returns the AI Mode tile icon from the DSE icon supplier when Aim3pEntrypoint is enabled (or
     * null if unavailable), or the default 1P spark icon when the feature is disabled.
     */
    @Override
    public @Nullable Drawable getAiModeTileIconDrawable() {
        if (mIsAim3pEntrypointEnabled) {
            AiModeButtonUiConfig config = assumeNonNull(mAiModeButtonUiConfigSupplier).get();
            if (config == null) return null;

            return assumeNonNull(mAiModeButtonIconSupplier).get().getDrawable(mContext);
        }

        return ResourcesCompat.getDrawable(
                mContext.getResources(), R.drawable.search_spark_white_24dp, /* theme= */ null);
    }
}
