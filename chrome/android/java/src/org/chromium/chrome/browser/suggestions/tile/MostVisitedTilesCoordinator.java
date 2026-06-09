// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static android.view.View.GONE;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}. */
@NullMarked
public class MostVisitedTilesCoordinator implements ConfigurationChangedObserver {
    private static final int TITLE_LINES = 1;
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final UiConfig mUiConfig;
    private final View mMvTilesContainerLayout;
    private final boolean mIsTablet;
    private MostVisitedTilesMediator mMediator;
    private @Nullable TileRenderer mRenderer;
    private @Nullable UserEducationHelper mUserEducationHelper;
    private @Nullable ContextMenuManager mContextMenuManager;
    private @Nullable OfflinePageBridge mOfflinePageBridge;

    /**
     * @param activity The app activity.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events,
     *     e.g.configuration changes. We need this to adjust the paddings and margins of the tile
     *     views.
     * @param mvTilesContainerLayout The container view of most visited tiles layout.
     * @param snapshotTileGridChangedRunnable The runnable called when the snapshot tile grid is
     *     changed.
     * @param tileCountChangedRunnable The runnable called when the tile count is changed.
     */
    public MostVisitedTilesCoordinator(
            Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            View mvTilesContainerLayout,
            @Nullable Runnable snapshotTileGridChangedRunnable,
            @Nullable Runnable tileCountChangedRunnable) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mMvTilesContainerLayout = mvTilesContainerLayout;
        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);

        MostVisitedTilesLayout tilesLayout =
                mvTilesContainerLayout.findViewById(R.id.mv_tiles_layout);

        mUiConfig = new UiConfig(tilesLayout);
        PropertyModel propertyModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                propertyModel,
                new MostVisitedTilesViewBinder.ViewHolder(mvTilesContainerLayout, tilesLayout),
                MostVisitedTilesViewBinder::bind);
        mRenderer =
                new TileRenderer(
                        mActivity,
                        SuggestionsConfig.getTileStyle(mUiConfig, mIsTablet),
                        TITLE_LINES,
                        /* imageFetcher= */ null);

        mMediator =
                new MostVisitedTilesMediator(
                        activity,
                        mUiConfig,
                        mvTilesContainerLayout,
                        mRenderer,
                        propertyModel,
                        mIsTablet,
                        snapshotTileGridChangedRunnable,
                        tileCountChangedRunnable);
    }

    /**
     * Called before the TasksSurface is showing to initialize MV tiles. {@link
     * MostVisitedTilesCoordinator#destroy()} is called after the TasksSurface hides.
     *
     * @param profile The Profile associated with the MV Tiles being displayed.
     * @param suggestionsUiDelegate The UI delegate of suggestion surface.
     * @param tileGroupDelegate The delegate of tile group.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *     events are allowed.
     */
    public void initWithNative(
            Profile profile,
            SuggestionsUiDelegate suggestionsUiDelegate,
            TileGroup.Delegate tileGroupDelegate,
            TouchEnabledDelegate touchEnabledDelegate) {
        mActivityLifecycleDispatcher.register(this);
        if (mRenderer == null) {
            mRenderer =
                    new TileRenderer(
                            mActivity,
                            SuggestionsConfig.getTileStyle(mUiConfig, mIsTablet),
                            /* titleLines= */ 1,
                            suggestionsUiDelegate.getImageFetcher());
        } else {
            mRenderer.setImageFetcher(suggestionsUiDelegate.getImageFetcher());
        }
        mRenderer.onNativeInitializationReady(profile);

        Handler handler = new Handler(Looper.getMainLooper());
        mUserEducationHelper = new UserEducationHelper(mActivity, profile, handler);

        mContextMenuManager =
                new ContextMenuManager(
                        suggestionsUiDelegate.getNavigationDelegate(),
                        touchEnabledDelegate,
                        mActivity::closeContextMenu,
                        CONTEXT_MENU_USER_ACTION_PREFIX);
        mOfflinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        mMediator.initWithNative(
                profile,
                mUserEducationHelper,
                suggestionsUiDelegate,
                mContextMenuManager,
                tileGroupDelegate,
                assumeNonNull(mOfflinePageBridge),
                mRenderer);
    }

    /** Updates the visibility of the Most Visited Tiles section. */
    public void updateMvtVisibility() {
        mMediator.updateMvtVisibility();
    }

    /**
     * Updates the width and margins of the MV tiles container.
     *
     * @param totalWidth The total width of the MV tiles layout.
     */
    public void updateMvtWidth(int totalWidth) {
        if (mMvTilesContainerLayout.getVisibility() == GONE) return;

        mMediator.updateMvtWidth(totalWidth);
    }

    /**
     * Updates the margins for the most visited tiles layout based on what is shown above it.
     *
     * @param shouldShowLogo Whether the logo is shown.
     * @param isWhiteBackgroundOnSearchBoxApplied Whether a white background is applied to the fake
     *     search box.
     * @param isTablet Whether the device is a tablet.
     */
    public void updateTilesLayoutMargins(boolean shouldShowLogo, boolean isTablet) {
        mMediator.updateTilesLayoutMargins(shouldShowLogo, isTablet);
    }

    /** Called when the TasksSurface is hidden or NewTabPageLayout is destroyed. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);

        if (mOfflinePageBridge != null) mOfflinePageBridge = null;
        if (mRenderer != null) mRenderer = null;
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
        // TODO(crbug.com/515150822): Investigate to see whether this logic also needs to be
        //  triggered by #onSizeChanged().
        mMediator.onConfigurationChanged();
    }

    public void setMediatorForTesting(MostVisitedTilesMediator mediator) {
        var oldValue = mediator;
        mMediator = mediator;
        ResettersForTesting.register(() -> mMediator = oldValue);
    }
}
