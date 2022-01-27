// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.base.Log;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.io.IOException;
import java.util.List;

/**
 * Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}.
 *
 * TODO(mattsimmons): Move logic and view manipulation into the mediator/viewbinder. (and add tests)
 */
public class MostVisitedListCoordinator implements TileGroup.Observer {
    private static final String TAG = "TopSites";
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";
    private static final int TITLE_LINES = 1;

    // There's a limit of 12 in {@link MostVisitedSitesBridge#setObserver}.
    private static final int MAX_RESULTS = 12;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final MvTilesLayout mMvTilesLayout;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final SnackbarManager mSnackbarManager;
    private TileGroup mTileGroup;
    private TileGroup.Delegate mTileGroupDelegate;
    private TileRenderer mRenderer;
    private SuggestionsUiDelegate mSuggestionsUiDelegate;
    private ContextMenuManager mContextMenuManager;
    private OfflinePageBridge mOfflinePageBridge;
    private SuggestionsNavigationDelegate mNavigationDelegate;
    private boolean mInitializationComplete;
    private ImageFetcher mImageFetcher;

    public MostVisitedListCoordinator(Activity activity, MvTilesLayout mvTilesLayout,
            PropertyModel propertyModel, SnackbarManager snackbarManager,
            WindowAndroid windowAndroid) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mMvTilesLayout = mvTilesLayout;
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, mMvTilesLayout, MostVisitedListViewBinder::bind);
        mSnackbarManager = snackbarManager;
    }

    public void initialize() {
        mRenderer =
                new TileRenderer(mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, null);

        // If it's a cold start and Instant Start is turned on, we render MV tiles placeholder here
        // pre-native.
        if (!mInitializationComplete
                && TabUiFeatureUtilities.supportInstantStart(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity), mActivity)) {
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

    /**
     * Called before the TasksSurface is showing to initialize MV tiles.
     * {@link MostVisitedListCoordinator#destroyMVTiles()} is called after the TasksSurface hides.
     */
    public void initWithNative(SuggestionsNavigationDelegate navigationDelegate) {
        Profile profile = Profile.getLastUsedRegularProfile();
        mNavigationDelegate = navigationDelegate;
        mImageFetcher = new ImageFetcher(profile);
        if (mRenderer == null) {
            // This function is never called in incognito mode.
            mRenderer = new TileRenderer(
                    mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, mImageFetcher);
        } else {
            mRenderer.setImageFetcher(mImageFetcher);
        }
        mSuggestionsUiDelegate = new MostVisitedSuggestionsUiDelegate(
                mNavigationDelegate, profile, mSnackbarManager);
        Runnable closeContextMenuCallback = mActivity::closeContextMenu;
        mContextMenuManager = new ContextMenuManager(mSuggestionsUiDelegate.getNavigationDelegate(),
                (enabled) -> {}, closeContextMenuCallback, CONTEXT_MENU_USER_ACTION_PREFIX);
        mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);
        mOfflinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
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

    /** Called when the TasksSurface is hidden. */
    public void destroyMVTiles() {
        mMvTilesLayout.destroy();

        if (mTileGroup != null) {
            mTileGroup.destroy();
            mTileGroup = null;
        }
        if (mTileGroupDelegate != null) {
            mTileGroupDelegate.destroy();
            mTileGroupDelegate = null;
        }
        mOfflinePageBridge = null;

        if (mWindowAndroid != null) {
            mWindowAndroid.removeContextMenuCloseListener(mContextMenuManager);
            mContextMenuManager = null;
        }
        if (mSuggestionsUiDelegate != null) {
            ((SuggestionsUiDelegateImpl) mSuggestionsUiDelegate).onDestroy();
            mSuggestionsUiDelegate = null;
        }

        mRenderer = null;

        if (mImageFetcher != null) {
            mImageFetcher.onDestroy();
        }
    }

    public boolean isMVTilesCleanedUp() {
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
