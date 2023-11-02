// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}.
 */
public class MostVisitedTilesCoordinator implements ConfigurationChangedObserver {
    private static final int TITLE_LINES = 1;
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";
    /**
     * The maximum number of tiles to try and fit in a row. On smaller screens, there may not be
     * enough space to fit all of them.
     */
    private static final int MAX_TILE_COLUMNS_FOR_GRID = 4;

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final MostVisitedTilesMediator mMediator;
    private final WindowAndroid mWindowAndroid;
    private final UiConfig mUiConfig;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private TileRenderer mRenderer;
    private ContextMenuManager mContextMenuManager;
    private OfflinePageBridge mOfflinePageBridge;

    /**
     * @param activity The app activity.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events,
     *                                    e.g.configuration changes. We need this to adjust the
     *                                    paddings and margins of the tile views.
     * @param mvTilesContainerLayout The container view of most visited tiles layout.
     * @param windowAndroid The current {@link WindowAndroid}
     * @param shouldShowSkeletonUIPreNative Whether to show the background icon for pre-native
     *                                      surface.
     * @param isScrollableMVTEnabled Whether scrollable MVT is enabled. If true  {@link
     *                               MostVisitedTilesCarouselLayout} is used; if false {@link
     *                               MostVisitedTilesGridLayout} is used.
     * @param maxRows The maximum number of rows to display. This will only be used for {@link
     *                MostVisitedTilesGridLayout}.
     * @param snapshotTileGridChangedRunnable The runnable called when the snapshot tile grid is
     *                                        changed.
     * @param tileCountChangedRunnable The runnable called when the tile count is changed.
     */
    public MostVisitedTilesCoordinator(Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher, View mvTilesContainerLayout,
            WindowAndroid windowAndroid, boolean shouldShowSkeletonUIPreNative,
            boolean isScrollableMVTEnabled, int maxRows,
            @Nullable Runnable snapshotTileGridChangedRunnable,
            @Nullable Runnable tileCountChangedRunnable) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mWindowAndroid = windowAndroid;

        ((ViewStub) mvTilesContainerLayout.findViewById(
                 isScrollableMVTEnabled ? R.id.mv_tiles_carousel_stub : R.id.mv_tiles_grid_stub))
                .inflate();
        ViewGroup tilesLayout = mvTilesContainerLayout.findViewById(R.id.mv_tiles_layout);

        if (!isScrollableMVTEnabled) {
            assert maxRows != Integer.MAX_VALUE;
            ((MostVisitedTilesGridLayout) tilesLayout).setMaxColumns(MAX_TILE_COLUMNS_FOR_GRID);
            ((MostVisitedTilesGridLayout) tilesLayout).setMaxRows(maxRows);
        }

        mUiConfig = new UiConfig(tilesLayout);
        PropertyModel propertyModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(propertyModel,
                new MostVisitedTilesViewBinder.ViewHolder(mvTilesContainerLayout, tilesLayout),
                MostVisitedTilesViewBinder::bind);
        mRenderer = new TileRenderer(
                mActivity, SuggestionsConfig.getTileStyle(mUiConfig), TITLE_LINES, null);

        mMediator = new MostVisitedTilesMediator(activity.getResources(), mUiConfig, tilesLayout,
                mvTilesContainerLayout.findViewById(R.id.tile_grid_placeholder_stub), mRenderer,
                propertyModel, shouldShowSkeletonUIPreNative, isScrollableMVTEnabled,
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity),
                snapshotTileGridChangedRunnable, tileCountChangedRunnable);
    }

    /**
     * Called before the TasksSurface is showing to initialize MV tiles.
     * {@link MostVisitedTilesCoordinator#destroyMvtiles()} is called after the TasksSurface hides.
     *
     * @param suggestionsUiDelegate The UI delegate of suggestion surface.
     * @param tileGroupDelegate The delegate of tile group.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *                             events are allowed.
     */
    public void initWithNative(SuggestionsUiDelegate suggestionsUiDelegate,
            TileGroup.Delegate tileGroupDelegate, TouchEnabledDelegate touchEnabledDelegate) {
        mActivityLifecycleDispatcher.register(this);
        Profile profile = Profile.getLastUsedRegularProfile();
        int titleLines =
                ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_TAB_PAGE_TILES_TITLE_WRAP_AROUND)
                ? 2
                : 1;
        if (mRenderer == null) {
            mRenderer = new TileRenderer(mActivity, SuggestionsConfig.getTileStyle(mUiConfig),
                    titleLines, suggestionsUiDelegate.getImageFetcher());
        } else {
            mRenderer.setImageFetcher(suggestionsUiDelegate.getImageFetcher());
            mRenderer.setTitleLines(titleLines);
        }
        mRenderer.onNativeInitializationReady();

        mContextMenuManager = new ContextMenuManager(suggestionsUiDelegate.getNavigationDelegate(),
                touchEnabledDelegate, mActivity::closeContextMenu, CONTEXT_MENU_USER_ACTION_PREFIX);
        mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);
        mOfflinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        mMediator.initWithNative(suggestionsUiDelegate, mContextMenuManager, tileGroupDelegate,
                mOfflinePageBridge, mRenderer);
    }

    /** Called when the TasksSurface is hidden or NewTabPageLayout is destroyed. */
    public void destroyMvtiles() {
        mActivityLifecycleDispatcher.unregister(this);

        if (mOfflinePageBridge != null) mOfflinePageBridge = null;
        if (mRenderer != null) mRenderer = null;

        if (mWindowAndroid != null) {
            mWindowAndroid.removeContextMenuCloseListener(mContextMenuManager);
            mContextMenuManager = null;
        }

        if (mMediator != null) mMediator.destroy();
    }

    public boolean isMVTilesCleanedUp() {
        return mMediator.isMVTilesCleanedUp();
    }

    public void onSwitchToForeground() {
        if (!isMVTilesCleanedUp()) mMediator.onSwitchToForeground();
    }

    /* ConfigurationChangedObserver implementation. */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mMediator.onConfigurationChanged();
        mUiConfig.updateDisplayStyle();
    }

    @VisibleForTesting
    public void onTemplateURLServiceChangedForTesting() {
        mMediator.onTemplateURLServiceChanged();
    }
}
