// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.SuggestTileType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.ViewUtils;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Utility class that renders {@link Tile}s into a provided {@link ViewGroup}, creating and
 * manipulating the views as needed.
 */
public class TileRenderer {
    private final Context mContext;
    private final Resources.Theme mTheme;
    private RoundedIconGenerator mIconGenerator;
    private ImageFetcher mImageFetcher;

    @TileStyle private final int mStyle;
    private final int mDesiredIconSize;
    private final int mMinIconSize;
    private final float mIconCornerRadius;
    private int mTitleLinesCount;
    private boolean mNativeInitializationComplete;
    private Profile mProfile;

    @LayoutRes private final int mLayout;

    @LayoutRes private final int mTopSitesLayout;

    private class LargeIconCallbackImpl implements LargeIconBridge.LargeIconCallback {
        private final WeakReference<Tile> mTile;
        private final Runnable mLoadCompleteCallback;

        private LargeIconCallbackImpl(Tile tile, Runnable loadCompleteCallback) {
            mTile = new WeakReference<>(tile);
            mLoadCompleteCallback = loadCompleteCallback;
        }

        @Override
        public void onLargeIconAvailable(
                @Nullable Bitmap icon,
                int fallbackColor,
                boolean isFallbackColorDefault,
                @IconType int iconType) {
            Tile tile = mTile.get();
            if (tile != null) { // Do nothing if the tile was removed.
                tile.setIconType(iconType);
                if (icon == null) {
                    setTileIconFromColor(tile, fallbackColor, isFallbackColorDefault);
                } else {
                    setTileIconFromBitmap(tile, icon);
                }
                if (mLoadCompleteCallback != null) mLoadCompleteCallback.run();
            }

            mTile.clear();
        }
    }

    public TileRenderer(
            Context context, @TileStyle int style, int titleLines, ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
        mStyle = style;
        mTitleLinesCount = titleLines;

        mContext = context;
        Resources res = context.getResources();
        mTheme = context.getTheme();
        mDesiredIconSize = res.getDimensionPixelSize(R.dimen.tile_view_icon_size);
        mIconCornerRadius = res.getDimension(R.dimen.tile_view_icon_corner_radius);
        int minIconSize = res.getDimensionPixelSize(R.dimen.tile_view_icon_min_size);

        // On ldpi devices, mDesiredIconSize could be even smaller than the global limit.
        mMinIconSize = Math.min(mDesiredIconSize, minIconSize);

        mLayout = getLayout();
        mTopSitesLayout = getTopSitesLayout();

        int iconColor = context.getColor(R.color.default_favicon_background_color);
        int iconTextSize = res.getDimensionPixelSize(R.dimen.tile_view_icon_text_size);
        mIconGenerator =
                new RoundedIconGenerator(
                        mDesiredIconSize,
                        mDesiredIconSize,
                        mDesiredIconSize / 2,
                        iconColor,
                        iconTextSize);
    }

    /**
     * Renders tile views in the given {@link ViewGroup}, reusing existing tile views where
     * possible because view inflation and icon loading are slow.
     * @param parent The layout to render the tile views into.
     * @param sectionTiles Tiles to render.
     * @param setupDelegate Delegate used to setup callbacks and listeners for the new views.
     */
    public void renderTileSection(
            List<Tile> sectionTiles, ViewGroup parent, TileGroup.TileSetupDelegate setupDelegate) {
        try (TraceEvent e = TraceEvent.scoped("TileRenderer.renderTileSection")) {
            // Map the old tile views by url so they can be reused later.
            Map<SiteSuggestion, SuggestionsTileView> oldTileViews = new HashMap<>();
            int childCount = parent.getChildCount();
            for (int i = 0; i < childCount; i++) {
                SuggestionsTileView tileView = (SuggestionsTileView) parent.getChildAt(i);
                oldTileViews.put(tileView.getData(), tileView);
            }

            // Remove all views from the layout because even if they are reused later they'll have
            // to be added back in the correct order.
            parent.removeAllViews();

            for (Tile tile : sectionTiles) {
                SuggestionsTileView tileView = oldTileViews.get(tile.getData());
                if (tileView == null) {
                    tileView = buildTileView(tile, parent, setupDelegate);
                }

                parent.addView(tileView);
            }
        }
    }

    public void setImageFetcher(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    /**
     * Override currently set maximum number of title lines.
     * @param titleLines The new max number of title lines to be shown under the tile icon.
     */
    public void setTitleLines(int titleLines) {
        mTitleLinesCount = titleLines;
    }

    /** Record that a tile was clicked for IPH reasons. */
    private void recordTileClickedForIPH(String eventName) {
        assert mProfile != null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.notifyEvent(eventName);
    }

    /**
     * Inflates a new tile view, initializes it, and loads an icon for it.
     * @param tile The tile that holds the data to populate the new tile view.
     * @param parentView The parent of the new tile view.
     * @param setupDelegate The delegate used to setup callbacks and listeners for the new view.
     * @return The new tile view.
     */
    @VisibleForTesting
    SuggestionsTileView buildTileView(
            Tile tile, ViewGroup parentView, TileGroup.TileSetupDelegate setupDelegate) {
        SuggestionsTileView tileView =
                (SuggestionsTileView)
                        LayoutInflater.from(parentView.getContext())
                                .inflate(mLayout, parentView, false);

        tileView.initialize(tile, mTitleLinesCount);

        if (!mNativeInitializationComplete || setupDelegate == null) {
            return tileView;
        }

        // Note: It is important that the callbacks below don't keep a reference to the tile or
        // modify them as there is no guarantee that the same tile would be used to update the view.
        updateIcon(tile, setupDelegate);
        updateContentDescription(tile, tileView);

        TileGroup.TileInteractionDelegate delegate =
                setupDelegate.createInteractionDelegate(tile, tileView);
        if (tile.getSource() == TileSource.HOMEPAGE) {
            delegate.setOnClickRunnable(
                    () -> {
                        recordTileClickedForIPH(EventConstants.HOMEPAGE_TILE_CLICKED);
                        RecordHistogram.recordEnumeratedHistogram(
                                "NewTabPage.SuggestTiles.SelectedTileType",
                                SuggestTileType.OTHER,
                                SuggestTileType.COUNT);
                    });
        } else if (isSearchTile(tile)) {
            delegate.setOnClickRunnable(
                    () -> {
                        RecordHistogram.recordEnumeratedHistogram(
                                "NewTabPage.SuggestTiles.SelectedTileType",
                                SuggestTileType.SEARCH,
                                SuggestTileType.COUNT);
                    });
            delegate.setOnRemoveRunnable(
                    () -> {
                        RecordHistogram.recordEnumeratedHistogram(
                                "NewTabPage.SuggestTiles.DeletedTileType",
                                SuggestTileType.SEARCH,
                                SuggestTileType.COUNT);
                    });
        } else {
            delegate.setOnClickRunnable(
                    () -> {
                        RecordHistogram.recordEnumeratedHistogram(
                                "NewTabPage.SuggestTiles.SelectedTileType",
                                SuggestTileType.URL,
                                SuggestTileType.COUNT);
                    });
            delegate.setOnRemoveRunnable(
                    () -> {
                        RecordHistogram.recordEnumeratedHistogram(
                                "NewTabPage.SuggestTiles.DeletedTileType",
                                SuggestTileType.URL,
                                SuggestTileType.COUNT);
                    });
        }

        tileView.setOnClickListener(delegate);
        tileView.setOnCreateContextMenuListener(delegate);

        return tileView;
    }

    /**
     * @return True, if the tile represents a Search query.
     */
    public boolean isSearchTile(Tile tile) {
        assert mProfile != null;
        TemplateUrlService searchService = TemplateUrlServiceFactory.getForProfile(mProfile);
        return searchService != null
                && searchService.isSearchResultsPageFromDefaultSearchProvider(tile.getUrl());
    }

    /**
     * Notify the component that the native initialization has completed and the component can
     * safely execute native code.
     */
    public void onNativeInitializationReady(Profile profile) {
        mNativeInitializationComplete = true;
        mProfile = profile;
    }

    /**
     * Given a Tile data and TileView, apply appropriate content description that will be announced
     * when the view is focused for accessibility. The objective of the description is to offer
     * audible guidance that helps users differentiate navigation (open www.site.com) and search
     * (search www.site.com).
     *
     * @param tile Tile data that carries information about the destination URL.
     * @param tileView The view that should receive updated content description.
     */
    private void updateContentDescription(Tile tile, SuggestionsTileView tileView) {
        if (isSearchTile(tile)) {
            tileView.setContentDescription(
                    mContext.getString(
                            R.string.accessibility_omnibox_most_visited_tile_search,
                            tile.getTitle()));
        } else {
            tileView.setContentDescription(
                    mContext.getString(
                            R.string.accessibility_omnibox_most_visited_tile_navigate,
                            tile.getTitle(),
                            tile.getUrl().getHost()));
        }
    }

    /**
     * Update tile decoration.
     *
     * @param tile Tile data that carries information about the target site.
     * @param setupDelegate The delegate used to setup callbacks and listeners for the new view.
     */
    public void updateIcon(final Tile tile, TileGroup.TileSetupDelegate setupDelegate) {
        if (isSearchTile(tile)) {
            // We already have an icon, and could trigger the update instantly.
            // Problem is, the TileView is likely not attached yet and the update would not be
            // properly reflected. Yield.
            final Runnable iconCallback = setupDelegate.createIconLoadCallback(tile);
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        setTileIconFromRes(tile, R.drawable.ic_suggestion_magnifier);
                        if (iconCallback != null) iconCallback.run();
                    });
        } else if (mImageFetcher != null) {
            mImageFetcher.makeLargeIconRequest(
                    tile.getUrl(),
                    mMinIconSize,
                    new LargeIconCallbackImpl(tile, setupDelegate.createIconLoadCallback(tile)));
        }
    }

    public void setTileIconFromBitmap(Tile tile, Bitmap icon) {
        int radius = Math.round(mIconCornerRadius * icon.getWidth() / mDesiredIconSize);
        RoundedBitmapDrawable roundedIcon =
                ViewUtils.createRoundedBitmapDrawable(mContext.getResources(), icon, radius);
        roundedIcon.setAntiAlias(true);
        roundedIcon.setFilterBitmap(true);

        tile.setIcon(roundedIcon);
        tile.setIconTint(null);
        tile.setType(TileVisualType.ICON_REAL);
    }

    public void setTileIconFromRes(Tile tile, @DrawableRes int res) {
        tile.setIcon(ResourcesCompat.getDrawable(mContext.getResources(), res, null));
        tile.setIconTint(ChromeColors.getSecondaryIconTint(mContext, /* isIncognito= */ false));
        tile.setType(TileVisualType.ICON_DEFAULT);
    }

    public void setTileIconFromColor(Tile tile, int fallbackColor, boolean isFallbackColorDefault) {
        mIconGenerator.setBackgroundColor(fallbackColor);
        Bitmap icon = mIconGenerator.generateIconForUrl(tile.getUrl());
        tile.setIcon(new BitmapDrawable(mContext.getResources(), icon));
        tile.setIconTint(null);
        tile.setType(
                isFallbackColorDefault ? TileVisualType.ICON_DEFAULT : TileVisualType.ICON_COLOR);
    }

    private @LayoutRes int getLayout() {
        switch (mStyle) {
            case TileStyle.MODERN:
                return R.layout.suggestions_tile_view;
            case TileStyle.MODERN_CONDENSED:
                return R.layout.suggestions_tile_view_condensed;
        }
        assert false;
        return 0;
    }

    private @LayoutRes int getTopSitesLayout() {
        switch (mStyle) {
            case TileStyle.MODERN:
                return R.layout.top_sites_tile_view;
            case TileStyle.MODERN_CONDENSED:
                return R.layout.top_sites_tile_view_condensed;
        }
        assert false;
        return 0;
    }

    public void setIconGeneratorForTesting(RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }
}
