// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_VISIBLE;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPageUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator for handling {@link MostVisitedTilesLayout} related logic. */
@NullMarked
public class MostVisitedTilesMediator implements TileGroup.Observer {

    /**
     * Score threshold for the first Top Sites Tiles to trigger IPH for MVT Customization. For
     * reference, if all visits occur today, the score for 2 visits is 1.12876; 3 visit is 1.39907.
     */
    private static final double MVT_CUSTOMIZATION_IPH_TILE_SCORE_THRESHOULD = 1.3;

    private final Context mContext;
    private final Resources mResources;
    private final UiConfig mUiConfig;
    private final View mMvTilesContainerLayout;
    private final MostVisitedTilesLayout mMvTilesLayout;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private final int mTileViewLandscapePadding;
    private final int mTileViewPortraitEdgePadding;
    private final @Nullable Runnable mSnapshotTileGridChangedRunnable;
    private final @Nullable Runnable mTileCountChangedRunnable;

    private @Nullable HomepageStateListener mMvtVisibilityListener;
    private int mTileViewPortraitIntervalPadding;

    private TileRenderer mRenderer;
    private TileGroup mTileGroup;
    private UserEducationHelper mUserEducationHelper;
    private boolean mMvtContentFits;

    private final int mLateralMarginSum;
    private final int mTileViewEdgePaddingForTablet;
    private final int mTileViewIntervalPaddingForTablet;

    public MostVisitedTilesMediator(
            Context context,
            UiConfig uiConfig,
            View mvTilesContainerLayout,
            TileRenderer renderer,
            PropertyModel propertyModel,
            boolean isTablet,
            @Nullable Runnable snapshotTileGridChangedRunnable,
            @Nullable Runnable tileCountChangedRunnable) {
        mContext = context;
        mResources = context.getResources();
        mUiConfig = uiConfig;
        mRenderer = renderer;
        mModel = propertyModel;
        mIsTablet = isTablet;
        mSnapshotTileGridChangedRunnable = snapshotTileGridChangedRunnable;
        mTileCountChangedRunnable = tileCountChangedRunnable;
        mMvTilesContainerLayout = mvTilesContainerLayout;
        mMvTilesLayout = mvTilesContainerLayout.findViewById(R.id.mv_tiles_layout);

        mTileViewLandscapePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape);
        mTileViewPortraitEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        mLateralMarginSum =
                mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin) * 2;

        mTileViewEdgePaddingForTablet =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        mTileViewIntervalPaddingForTablet =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);

        maybeSetPortraitIntervalPaddings();

        addMostVisitedTilesVisibilityListener();
    }

    private void addMostVisitedTilesVisibilityListener() {
        mMvtVisibilityListener =
                new HomepageStateListener() {
                    @Override
                    public void onMvtToggleChanged() {
                        updateMvtVisibility();
                    }
                };
        NtpCustomizationConfigManager.getInstance()
                .addListener(mMvtVisibilityListener, mContext, /* skipNotify= */ false);
    }

    /** Called to initialize this mediator when native is ready. */
    @Initializer
    public void initWithNative(
            Profile profile,
            UserEducationHelper userEducationHelper,
            SuggestionsUiDelegate suggestionsUiDelegate,
            ContextMenuManager contextMenuManager,
            TileGroup.Delegate tileGroupDelegate,
            OfflinePageBridge offlinePageBridge,
            TileRenderer renderer) {
        mUserEducationHelper = userEducationHelper;
        mRenderer = renderer;
        mTileGroup =
                new TileGroup(
                        renderer,
                        suggestionsUiDelegate,
                        contextMenuManager,
                        tileGroupDelegate,
                        new TileDragDelegateImpl(mMvTilesLayout),
                        /* observer= */ this,
                        offlinePageBridge);
        mTileGroup.startObserving(SuggestionsConfig.MAX_TILE_COUNT);
    }

    /* TileGroup.Observer implementation. */
    @Override
    public void onTileDataChanged() {
        if (mTileGroup.getTileSections().size() < 1) return;

        List<Tile> tiles = mTileGroup.getTileSections().get(TileSectionType.PERSONALIZED);
        assumeNonNull(tiles);
        mRenderer.renderTileSection(tiles, mMvTilesLayout, mTileGroup.getTileSetupDelegate());
        mTileGroup.notifyTilesRendered();
        updateTilesView();

        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
        MostVisitedSitesMetadataUtils.getInstance().saveSuggestionListsToFile(tiles);

        maybeTriggerCustomizationIph(tiles);
    }

    @Override
    public void onTileCountChanged() {
        if (mTileCountChangedRunnable != null) mTileCountChangedRunnable.run();

        updateMvtVisibility();
    }

    /**
     * Sets the visibility of the Most Visited Tiles (MVT) section.
     *
     * <p>The visibility of MVT is controlled by a user accessible toggle. The section will be
     * visible if and only if the user has the toggle turned on.
     */
    void updateMvtVisibility() {
        mModel.set(IS_VISIBLE, NtpCustomizationConfigManager.getInstance().getPrefIsMvtToggleOn());
    }

    @Override
    public void onTileIconChanged(Tile tile) {
        updateTileIcon(tile);
        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
    }

    @Override
    public void onTileOfflineBadgeVisibilityChanged(Tile tile) {
        updateOfflineBadge(tile);
        if (mSnapshotTileGridChangedRunnable != null) mSnapshotTileGridChangedRunnable.run();
    }

    @Override
    public void onCustomTileCreation(Tile tile) {
        mMvTilesLayout.ensureTileIsInViewOnNextLayout(tile.getIndex());
    }

    @Override
    public void onCustomTileReorder(int newPos) {
        mMvTilesLayout.ensureTileIsInViewOnNextLayout(newPos);
    }

    public void onConfigurationChanged() {
        maybeSetPortraitIntervalPaddings();
        updateTilesView();
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mMvTilesLayout != null) {
            mMvTilesLayout.destroy();
        }

        if (mTileGroup != null) {
            mTileGroup.destroy();
            mTileGroup = null;
        }

        if (mMvtVisibilityListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mMvtVisibilityListener);
        }
    }

    public boolean isMVTilesCleanedUp() {
        return mTileGroup == null;
    }

    /**
     * Updates whether the MV tiles layout stays in the center of the container when used in NTP on
     * the tablet by changing the width of its container. Also updates the lateral margins.
     *
     * @param totalWidth The total width of the MV tiles layout. If it isn't null, we will
     *     recalculate the value of |mMvtContentFits|.
     */
    void updateMvtOnTablet(@Nullable Integer totalWidth) {
        if (totalWidth != null) {
            mMvtContentFits = mMvTilesLayout.contentFitsOnTablet(totalWidth);
        }

        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mMvTilesContainerLayout.getLayoutParams();
        marginLayoutParams.width =
                mMvtContentFits
                        ? ViewGroup.LayoutParams.WRAP_CONTENT
                        : ViewGroup.LayoutParams.MATCH_PARENT;

        int lateralPaddingId =
                NtpCustomizationUtils.isInNarrowWindowOnTablet(mIsTablet, mUiConfig)
                        ? R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet
                        : R.dimen.mvt_container_lateral_margin;
        int lateralPaddingsForNtp = mResources.getDimensionPixelSize(lateralPaddingId);
        marginLayoutParams.leftMargin = lateralPaddingsForNtp;
        marginLayoutParams.rightMargin = lateralPaddingsForNtp;
    }

    /**
     * Updates the margins for the most visited tiles layout based on what is shown above it.
     *
     * @param shouldShowLogo Whether the logo is shown.
     * @param isWhiteBackgroundOnSearchBoxApplied Whether a white background is applied to the fake
     *     search box.
     * @param isTablet Whether the device is a tablet.
     */
    void updateTilesLayoutMargins(boolean shouldShowLogo, boolean isTablet) {
        NewTabPageUtils.updateTilesLayoutTopMargin(
                mMvTilesContainerLayout, shouldShowLogo, isTablet);
    }

    public void onSwitchToForeground() {
        mTileGroup.onSwitchToForeground(/* trackLoadTask= */ false);
    }

    private void updateTileIcon(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) {
            tileView.renderIcon(tile);
        }
    }

    private void updateOfflineBadge(Tile tile) {
        SuggestionsTileView tileView = findTileView(tile.getData());
        if (tileView != null) tileView.renderOfflineBadge(tile);
    }

    private @Nullable SuggestionsTileView findTileView(SiteSuggestion data) {
        int tileCount = mMvTilesLayout.getTileCount();
        for (int i = 0; i < tileCount; i++) {
            SuggestionsTileView tileView = (SuggestionsTileView) mMvTilesLayout.getTileAt(i);
            if (data.equals(tileView.getData())) return tileView;
        }
        return null;
    }

    private void maybeSetPortraitIntervalPaddings() {
        if (mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE
                || mTileViewPortraitIntervalPadding != 0) {
            return;
        }
        if (!mIsTablet) {
            boolean isSmallDevice = mUiConfig.getCurrentDisplayStyle().isSmall();
            int screenWidth = mResources.getDisplayMetrics().widthPixels - mLateralMarginSum;
            int tileViewWidth =
                    mResources.getDimensionPixelOffset(
                            isSmallDevice
                                    ? R.dimen.tile_view_width_condensed
                                    : R.dimen.tile_view_width);
            // We want to show four and a half tile view to make users know the MV tiles are
            // scrollable. But the padding should be equal to or larger than tile_view_padding,
            // otherwise the titles among tiles would be overlapped.
            mTileViewPortraitIntervalPadding =
                    Integer.max(
                            -mResources.getDimensionPixelOffset(R.dimen.tile_view_padding),
                            (int)
                                    ((screenWidth
                                                    - mTileViewPortraitEdgePadding
                                                    - tileViewWidth * 4.5)
                                            / 4));
        }
    }

    private void updateTilesView() {
        // Skip if no children (tile or otherwise).
        if (mMvTilesLayout.getChildCount() < 1) return;

        if (mIsTablet) {
            mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewEdgePaddingForTablet);
            mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewIntervalPaddingForTablet);
            return;
        }

        if (mResources.getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewLandscapePadding);
            mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewLandscapePadding);
            return;
        }

        mModel.set(HORIZONTAL_EDGE_PADDINGS, mTileViewPortraitEdgePadding);
        mModel.set(HORIZONTAL_INTERVAL_PADDINGS, mTileViewPortraitIntervalPadding);
    }

    private void maybeTriggerCustomizationIph(List<Tile> tiles) {
        if (!ChromeFeatureList.sMostVisitedTilesCustomization.isEnabled()) return;

        if (tiles.size() == 0) return;

        Tile firstTile = tiles.get(0);
        if (firstTile.getData().source == TileSource.CUSTOM_LINKS) return;

        double firstTileScore = mTileGroup.getSuggestionScore(firstTile.getUrl());
        if (firstTileScore < MVT_CUSTOMIZATION_IPH_TILE_SCORE_THRESHOULD) return;

        mMvTilesLayout.triggerCustomizationIph(mUserEducationHelper);
    }
}
