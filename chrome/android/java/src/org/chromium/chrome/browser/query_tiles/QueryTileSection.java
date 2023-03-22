// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTile;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinator;
import org.chromium.components.browser_ui.widget.image_tiles.ImageTileCoordinatorFactory;
import org.chromium.components.browser_ui.widget.image_tiles.TileConfig;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.QueryTileConstants;
import org.chromium.components.query_tiles.TileProvider;
import org.chromium.components.query_tiles.TileUmaLogger;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Represents the query tiles section on the new tab page. Abstracts away the general tasks related
 * to initializing and fetching data for the UI and making decisions whether to show or hide this
 * section.
 */
public class QueryTileSection {
    private static final String UMA_PREFIX = "QueryTiles.NTP";
    private static final String MOST_VISITED_MAX_ROWS_SMALL_SCREEN =
            "most_visited_max_rows_small_screen";
    private static final String MOST_VISITED_MAX_ROWS_NORMAL_SCREEN =
            "most_visited_max_rows_normal_screen";
    private static final String VARIATION_SMALL_SCREEN_HEIGHT_THRESHOLD_DP =
            "small_screen_height_threshold_dp";
    private static final int DEFAULT_SMALL_SCREEN_HEIGHT_THRESHOLD_DP = 700;
    private static final int DEFAULT_MOST_VISITED_MAX_ROWS = 1;

    private final ViewGroup mQueryTileSectionView;
    private final Callback<QueryInfo> mSubmitQueryCallback;
    private ImageTileCoordinator mTileCoordinator;
    private TileProvider mTileProvider;
    private TileUmaLogger mTileUmaLogger;
    private ImageFetcher mImageFetcher;
    private Integer mTileWidth;
    private float mAnimationPercent;
    private boolean mNeedReload = true;

    /**
     * Represents the information needed to launch a search query when clicking on a tile.
     */
    public static class QueryInfo {
        public final String queryText;
        public final List<String> searchParams;

        public QueryInfo(String queryText, List<String> searchParams) {
            this.queryText = queryText;
            this.searchParams = searchParams;
        }
    }

    /** Constructor. */
    public QueryTileSection(ViewGroup queryTileSectionView, Profile profile,
            Callback<QueryInfo> performSearchQueryCallback) {
        mQueryTileSectionView = queryTileSectionView;
        mSubmitQueryCallback = performSearchQueryCallback;

        mTileProvider = TileProviderFactory.getForProfile(profile);
        TileConfig tileConfig = new TileConfig.Builder().setUmaPrefix(UMA_PREFIX).build();
        mTileUmaLogger = new TileUmaLogger(UMA_PREFIX);
        mTileCoordinator = ImageTileCoordinatorFactory.create(mQueryTileSectionView.getContext(),
                tileConfig, this::onTileClicked, this::getVisuals);
        mQueryTileSectionView.addView(mTileCoordinator.getView(),
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(), GlobalDiscardableReferencePool.getReferencePool());
        reloadTiles();
    }

    /**
     * Called to notify the animation progress for transition between fake search box and omnibox.
     * @param percent The animation progress.
     */
    public void onUrlFocusAnimationChanged(float percent) {
        if (mAnimationPercent == percent) return;
        mAnimationPercent = percent;
        if (percent == 0) reloadTiles();
    }

    /**
     * Called to clear the state and reload the top level tiles. Any chip selected will be cleared.
     */
    public void reloadTiles() {
        if (!mNeedReload) {
            // No tile changes, just refresh the display.
            mTileCoordinator.refreshTiles();
            return;
        }
        // TODO(qinmin): don't return all tiles, just return the top-level tiles.
        mTileProvider.getQueryTiles(null, this::setTiles);
        mNeedReload = false;
    }

    private void onTileClicked(ImageTile tile) {
        QueryTile queryTile = (QueryTile) tile;
        mTileUmaLogger.recordTileClicked(queryTile);
        mTileProvider.onTileClicked(queryTile.id);
        QueryTileUtils.onQueryTileClicked();

        // TODO(qinmin): make isLastLevelTile a member variable of ImageTile.
        boolean isLastLevelTile = queryTile.children.isEmpty();
        if (isLastLevelTile) {
            mSubmitQueryCallback.onResult(
                    new QueryInfo(queryTile.queryText, queryTile.searchParams));
            return;
        }

        mNeedReload = true;
        mTileProvider.getQueryTiles(tile.id, this::setTiles);
    }

    private void setTiles(List<QueryTile> tiles) {
        mTileUmaLogger.recordTilesLoaded(tiles);
        mTileCoordinator.setTiles(new ArrayList<>(tiles));
        mQueryTileSectionView.setVisibility(tiles.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void getVisuals(ImageTile tile, Callback<List<Bitmap>> callback) {
        // TODO(crbug.com/1077086): Probably need a bigger width to start with or pass the exact
        // width. Also may need to update on orientation change.
        if (mTileWidth == null) {
            mTileWidth = mQueryTileSectionView.getResources().getDimensionPixelSize(
                    R.dimen.tile_ideal_width);
        }

        fetchImage((QueryTile) tile, mTileWidth,
                bitmap -> { callback.onResult(Arrays.asList(bitmap)); });
    }

    private void fetchImage(QueryTile queryTile, int size, Callback<Bitmap> callback) {
        if (queryTile.urls.isEmpty()) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(null));
            return;
        }

        GURL url = queryTile.urls.get(0);
        ImageFetcher.Params params = ImageFetcher.Params.createWithExpirationInterval(url,
                ImageFetcher.QUERY_TILE_UMA_CLIENT_NAME, size, size,
                QueryTileConstants.IMAGE_EXPIRATION_INTERVAL_MINUTES);
        mImageFetcher.fetchImage(params, callback);
    }

    /**
     * @param context Context for display calculation.
     * @return Max number of rows for most visited tiles. For smaller screens, the most visited
     *         tiles section on NTP is shortened so that feed is still visible above the fold.
     */
    public static int getMaxRowsForMostVisitedTiles(Context context) {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(context);
        int screenHeightDp = DisplayUtil.pxToDp(display, display.getDisplayHeight());
        int smallScreenHeightThresholdDp = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.QUERY_TILES, VARIATION_SMALL_SCREEN_HEIGHT_THRESHOLD_DP,
                DEFAULT_SMALL_SCREEN_HEIGHT_THRESHOLD_DP);
        boolean isSmallScreen = screenHeightDp < smallScreenHeightThresholdDp;
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(ChromeFeatureList.QUERY_TILES,
                isSmallScreen ? MOST_VISITED_MAX_ROWS_SMALL_SCREEN
                              : MOST_VISITED_MAX_ROWS_NORMAL_SCREEN,
                DEFAULT_MOST_VISITED_MAX_ROWS);
    }
}
