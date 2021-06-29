// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.suggestions.tile.TileGroup;
import org.chromium.chrome.browser.suggestions.tile.TileGroupDelegateImpl;
import org.chromium.chrome.browser.suggestions.tile.TileRenderer;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.io.IOException;
import java.util.List;

/**
 * Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}.
 *
 * TODO(mattsimmons): Move logic and view manipulation into the mediator/viewbinder. (and add tests)
 */
class MostVisitedListCoordinator implements TileGroup.Observer {
    private static final String TAG = "TopSites";
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";
    private static final int TITLE_LINES = 1;

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    private static final int MAX_RESULTS = 12;
    private final Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private final MvTilesLayout mMvTilesLayout;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final Supplier<Tab> mParentTabSupplier;
    private final SnackbarManager mSnackbarManager;
    private TileGroup mTileGroup;
    private TileGroup.Delegate mTileGroupDelegate;
    private TileRenderer mRenderer;
    private SuggestionsUiDelegate mSuggestionsUiDelegate;
    private ContextMenuManager mContextMenuManager;
    private OfflinePageBridge mOfflinePageBridge;
    private SuggestionsNavigationDelegate mNavigationDelegate;
    private boolean mInitializationComplete;

    public MostVisitedListCoordinator(Activity activity, MvTilesLayout mvTilesLayout,
            PropertyModel propertyModel, Supplier<Tab> parentTabSupplier,
            SnackbarManager snackbarManager, WindowAndroid windowAndroid) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mMvTilesLayout = mvTilesLayout;
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, mMvTilesLayout, MostVisitedListViewBinder::bind);
        mParentTabSupplier = parentTabSupplier;
        mSnackbarManager = snackbarManager;
    }

    public void initialize() {
        mRenderer =
                new TileRenderer(mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, null);

        // If it's a cold start and Instant Start is turned on, we render MV tiles placeholder here
        // pre-native.
        if (!mInitializationComplete
                && ReturnToChromeExperimentsUtil.isStartSurfaceHomepageEnabled()
                && TabUiFeatureUtilities.supportInstantStart(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity))) {
            try {
                List<Tile> tiles =
                        MostVisitedSitesMetadataUtils.restoreFileToSuggestionListsOnUiThread();
                if (tiles != null) {
                    mRenderer.renderTileSection(tiles, mMvTilesLayout, null);
                    mMvTilesLayout.updateTilesViewLayout(
                            mActivity.getResources().getConfiguration().orientation);
                }
            } catch (IOException e) {
                Log.i(TAG, "No cached MV tiles file.");
            }
        }
    }

    public void initWithNative() {
        Profile profile = Profile.getLastUsedRegularProfile();
        if (!mInitializationComplete) {
            ImageFetcher imageFetcher = new ImageFetcher(profile);
            if (mRenderer == null) {
                // This function is never called in incognito mode.
                mRenderer = new TileRenderer(
                        mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, imageFetcher);
            } else {
                mRenderer.setImageFetcher(imageFetcher);
            }
            mNavigationDelegate = new MostVisitedTileNavigationDelegate(
                    mActivity, profile, null, null, null, mParentTabSupplier);
            mSuggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(
                    mNavigationDelegate, profile, mSnackbarManager);
            Runnable closeContextMenuCallback = mActivity::closeContextMenu;
            mContextMenuManager = new ContextMenuManager(
                    mSuggestionsUiDelegate.getNavigationDelegate(),
                    (enabled) -> {}, closeContextMenuCallback, CONTEXT_MENU_USER_ACTION_PREFIX);
            mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);
            mOfflinePageBridge =
                    SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        }
        mTileGroupDelegate = new TileGroupDelegateImpl(
                mActivity, profile, mNavigationDelegate, mSnackbarManager);
        mTileGroup = new TileGroup(mRenderer, mSuggestionsUiDelegate, mContextMenuManager,
                mTileGroupDelegate, this, mOfflinePageBridge);
        mTileGroup.startObserving(MAX_RESULTS);
        mInitializationComplete = true;
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

    /** TileGroup.Observer implementation. */
    @Override
    public void onTileDataChanged() {
        if (mTileGroup.getTileSections().size() < 1) return;

        mRenderer.renderTileSection(mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED),
                mMvTilesLayout, mTileGroup.getTileSetupDelegate());
        mMvTilesLayout.updateTilesViewLayout(
                mActivity.getResources().getConfiguration().orientation);

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

    private static class MostVisitedTileNavigationDelegate extends SuggestionsNavigationDelegate {
        private Supplier<Tab> mParentTabSupplier;
        private TabDelegate mTabDelegate;

        public MostVisitedTileNavigationDelegate(Activity activity, Profile profile,
                NativePageHost host, TabModelSelector tabModelSelector, Tab tab,
                Supplier<Tab> parentTabSupplier) {
            super(activity, profile, host, tabModelSelector, tab);
            mParentTabSupplier = parentTabSupplier;
            mTabDelegate = new TabDelegate(false);
        }

        @Override
        public boolean isOpenInNewTabInGroupEnabled() {
            return false;
        }

        @Override
        public void navigateToHelpPage() {
            // TODO(dgn): Use the standard Help UI rather than a random link to online help?
            ReturnToChromeExperimentsUtil.handleLoadUrlFromStartSurface(
                    new LoadUrlParams(NEW_TAB_URL_HELP, PageTransition.AUTO_BOOKMARK),
                    true /*incognito*/, mParentTabSupplier.get());
        }

        /**
         * Opens the suggestions page without recording metrics.
         *
         * @param windowOpenDisposition How to open (new window, current tab, etc).
         * @param url The url to navigate to.
         */
        @Override
        public void navigateToSuggestionUrl(
                int windowOpenDisposition, String url, boolean inGroup) {
            assert !inGroup;
            switch (windowOpenDisposition) {
                case WindowOpenDisposition.CURRENT_TAB:
                case WindowOpenDisposition.NEW_BACKGROUND_TAB:
                    ReturnToChromeExperimentsUtil.handleLoadUrlFromStartSurface(
                            new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK),
                            null /*incognito*/, mParentTabSupplier.get());
                    SuggestionsMetrics.recordTileTapped();
                    break;
                case WindowOpenDisposition.OFF_THE_RECORD:
                    ReturnToChromeExperimentsUtil.handleLoadUrlFromStartSurface(
                            new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK),
                            true /*incognito*/, mParentTabSupplier.get());
                    break;
                case WindowOpenDisposition.NEW_WINDOW:
                    openUrlInNewWindow(new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK));
                    break;
                case WindowOpenDisposition.SAVE_TO_DISK:
                    // TODO(crbug.com/1202321): Downloading toast is not shown maybe due to the
                    // webContent is null for start surface.
                    saveUrlForOffline(url);
                    break;
                default:
                    assert false;
            }
        }

        private void saveUrlForOffline(String url) {
            // TODO(crbug.com/1193816): Namespace shouldn't be NTP_SUGGESTIONS_NAMESPACE since it's
            // not on NTP.
            RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                    .savePageLater(url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE,
                            true /* userRequested */);
        }

        private void openUrlInNewWindow(LoadUrlParams loadUrlParams) {
            mTabDelegate.createTabInOtherWindow(loadUrlParams, mActivity,
                    mParentTabSupplier.get() == null ? -1 : mParentTabSupplier.get().getId());
        }
    }

    /** Called when the TasksSurface is hidden. */
    public void destroyMVTiles() {
        if (mTileGroupDelegate != null) {
            mTileGroupDelegate.destroy();
            mTileGroupDelegate = null;
        }
        mTileGroup = null;
        mMvTilesLayout.removeAllViews();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean isMVTilesCleanedUp() {
        return mTileGroupDelegate == null && mTileGroup == null;
    }

    /** Suggestions UI Delegate for constructing the TileGroup. */
    private static class MostVisitedSuggestionsUiDelegate extends SuggestionsUiDelegateImpl {
        public MostVisitedSuggestionsUiDelegate(SuggestionsNavigationDelegate navigationDelegate,
                Profile profile, SnackbarManager snackbarManager) {
            super(navigationDelegate, profile, null, snackbarManager);
        }

        @Override
        public boolean isVisible() {
            return false;
        }
    }
}
