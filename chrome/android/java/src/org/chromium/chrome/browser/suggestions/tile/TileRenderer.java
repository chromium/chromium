// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorStateListDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.RoundedBitmapDrawable;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.suggestions.mostvisited.SuggestTileType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ViewUtils;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/**
 * Utility class that renders {@link Tile}s into a provided {@link TilesLinearLayout}, creating and
 * manipulating the views as needed.
 */
public class TileRenderer {
    private final Context mContext;
    private RoundedIconGenerator mIconGenerator;
    private ImageFetcher mImageFetcher;

    @TileStyle private final int mStyle;
    private final int mDesiredIconSize;
    private final int mMinIconSize;
    private final float mIconCornerRadius;
    private int mTitleLinesCount;
    private boolean mNativeInitializationComplete;
    private Profile mProfile;

    @LayoutRes private final int mTileLayoutResId;
    private final float mTileWidthDp;
    private final float mDividerWidthDp;

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

    /** Simple multimap from SiteSuggestion to SuggestionsTileView. */
    private static class SuggestionsTileViewCache {
        private final Map<SiteSuggestion, LinkedList<SuggestionsTileView>> mStorage =
                new HashMap<SiteSuggestion, LinkedList<SuggestionsTileView>>();

        void put(SiteSuggestion key, @NonNull SuggestionsTileView value) {
            LinkedList<SuggestionsTileView> bucket = mStorage.get(key);
            if (bucket == null) {
                bucket = new LinkedList<SuggestionsTileView>();
                mStorage.put(key, bucket);
            }
            bucket.addLast(value);
        }

        @Nullable
        SuggestionsTileView remove(SiteSuggestion key) {
            SuggestionsTileView ret = null;
            LinkedList<SuggestionsTileView> bucket = mStorage.get(key);
            if (bucket != null) {
                ret = bucket.removeFirst(); // FIFO, for consistecy.
                if (bucket.isEmpty()) {
                    mStorage.remove(key);
                }
            }
            return ret;
        }
    }

    public TileRenderer(
            Context context, @TileStyle int style, int titleLines, ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
        mStyle = style;
        mTitleLinesCount = titleLines;

        mContext = context;
        Resources res = mContext.getResources();
        mDesiredIconSize = res.getDimensionPixelSize(R.dimen.tile_view_icon_size);
        mIconCornerRadius = res.getDimension(R.dimen.tile_view_icon_corner_radius);
        int minIconSize = res.getDimensionPixelSize(R.dimen.tile_view_icon_min_size);

        // On ldpi devices, mDesiredIconSize could be even smaller than the global limit.
        mMinIconSize = Math.min(mDesiredIconSize, minIconSize);

        mTileLayoutResId = getTileLayoutResId();
        mTileWidthDp = res.getDimension(getTileWidthDimenResId());
        mDividerWidthDp = res.getDimension(R.dimen.tile_view_divider_width);

        int iconColor = mContext.getColor(R.color.default_favicon_background_color);
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
     * Renders tile views in the given {@link TilesLinearLayout}, reusing existing tile views where
     * possible because view inflation and icon loading are slow.
     *
     * @param parent The layout to render the tile views into.
     * @param sectionTiles Tiles to render.
     * @param setupDelegate Delegate used to setup callbacks and listeners for the new views.
     */
    public void renderTileSection(
            List<Tile> sectionTiles,
            TilesLinearLayout parent,
            TileGroup.TileSetupDelegate setupDelegate) {
        try (TraceEvent e = TraceEvent.scoped("TileRenderer.renderTileSection")) {
            // Map the old tile views by url so they can be reused later.
            SuggestionsTileViewCache oldTileViews = new SuggestionsTileViewCache();
            int tileCount = parent.getTileCount();
            for (int i = 0; i < tileCount; i++) {
                SuggestionsTileView tileView = (SuggestionsTileView) parent.getTileAt(i);
                oldTileViews.put(tileView.getData(), tileView);
            }

            // Remove all views from the layout because even if they are reused later they'll have
            // to be added back in the correct order.
            parent.removeAllViews();

            Tile prevTile = null;
            for (Tile tile : sectionTiles) {
                SuggestionsTileView tileView = oldTileViews.remove(tile.getData());
                if (tileView == null) {
                    tileView = buildTileView(tile, parent, setupDelegate);
                }
                // Add divider if sources change between CUSTOM_LINKS and any other type.
                if (prevTile != null
                        && (prevTile.getData().source == TileSource.CUSTOM_LINKS)
                                != (tile.getData().source == TileSource.CUSTOM_LINKS)) {
                    parent.addNonTileViewWithWidth(buildDivider(parent), mDividerWidthDp);
                }
                parent.addTile(tileView);
                prevTile = tile;
            }

            if (shouldShowAddNewButton(sectionTiles)) {
                TileView addCustomLinksButton = buildAddCustomLinksButton(parent, setupDelegate);
                parent.addNonTileViewWithWidth(addCustomLinksButton, mTileWidthDp);
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
    private void recordTileClickedForIph(String eventName) {
        assert mProfile != null;
        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.notifyEvent(eventName);
    }

    /**
     * Inflates a new tile view, initializes it, and loads an icon for it.
     *
     * @param tile The tile that holds the data to populate the new tile view.
     * @param parent The parent of the new tile view.
     * @param setupDelegate The delegate used to setup callbacks and listeners for the new view.
     * @return The new tile view.
     */
    @VisibleForTesting
    SuggestionsTileView buildTileView(
            Tile tile, TilesLinearLayout parent, TileGroup.TileSetupDelegate setupDelegate) {
        SuggestionsTileView tileView =
                (SuggestionsTileView)
                        LayoutInflater.from(parent.getContext())
                                .inflate(mTileLayoutResId, parent, false);

        tileView.initialize(tile, mTitleLinesCount);
        // TODO(crbug.com/403353768): Unify tile background.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            tileView.setBackground(
                    new ColorStateListDrawable(
                            AppCompatResources.getColorStateList(
                                    parent.getContext(), R.color.tile_bg_color_list)));
        }

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
                        recordTileClickedForIph(EventConstants.HOMEPAGE_TILE_CLICKED);
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TILE_CONTEXT_MENU_REFACTOR)) {
            tileView.setOnLongClickListener(delegate);
        } else {
            tileView.setOnCreateContextMenuListener(delegate);
        }

        return tileView;
    }

    View buildDivider(TilesLinearLayout parent) {
        return (View)
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.suggestions_tile_vertical_divider, parent, false);
    }

    boolean shouldShowAddNewButton(List<Tile> sectionTiles) {
        if (!ChromeFeatureList.sMostVisitedTilesCustomization.isEnabled()) {
            return false;
        }

        if (sectionTiles.size() == 0) {
            // Still show the Add Custom Link Button, even if no suggestions exist. We might make
            // this configurable.
            return true;
        }

        return TileUtils.customTileCountIsUnderLimit(sectionTiles);
    }

    TileView buildAddCustomLinksButton(
            TilesLinearLayout parent, TileGroup.TileSetupDelegate setupDelegate) {
        Resources res = mContext.getResources();
        String title = res.getString(R.string.most_visited_add_new);
        Drawable plusIcon =
                ResourcesCompat.getDrawable(mContext.getResources(), R.drawable.plus, null);
        TileView tileView =
                (TileView)
                        LayoutInflater.from(parent.getContext())
                                .inflate(mTileLayoutResId, parent, false);
        tileView.initialize(title, /* showOfflineBadge= */ false, plusIcon, mTitleLinesCount);
        tileView.setIconTint(
                ChromeColors.getSecondaryIconTint(mContext, /* forceLightIconTint= */ false));
        tileView.setContentDescription(
                mContext.getString(
                        R.string.accessibility_omnibox_most_visited_tile_add_new_shortcut));
        tileView.setOnClickListener(
                (View v) -> {
                    RecordUserAction.record("Suggestions.Button.AddItem");
                    setupDelegate.getCustomTileModificationDelegate().add();
                });
        return tileView;
    }

    /** Returns whether the tile represents a Search query. */
    private boolean isSearchTile(Tile tile) {
        return TileUtils.isSearchTile(mProfile, tile);
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
            setTileIconFromResAsync(tile, setupDelegate, R.drawable.ic_suggestion_magnifier);

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

    public void setTileIconFromResAsync(
            Tile tile, TileGroup.TileSetupDelegate setupDelegate, @DrawableRes int res) {
        // We already have an icon, and could trigger the update instantly.
        // Problem is, the TileView is likely not attached yet and the update would not be
        // properly reflected. Yield.
        final Runnable iconCallback = setupDelegate.createIconLoadCallback(tile);
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    tile.setIcon(ResourcesCompat.getDrawable(mContext.getResources(), res, null));
                    tile.setIconTint(
                            ChromeColors.getSecondaryIconTint(
                                    mContext, /* forceLightIconTint= */ false));
                    tile.setType(TileVisualType.ICON_DEFAULT);
                    if (iconCallback != null) iconCallback.run();
                });
    }

    public void setTileIconFromColor(Tile tile, int fallbackColor, boolean isFallbackColorDefault) {
        mIconGenerator.setBackgroundColor(fallbackColor);
        Bitmap icon = mIconGenerator.generateIconForUrl(tile.getUrl());
        tile.setIcon(new BitmapDrawable(mContext.getResources(), icon));
        tile.setIconTint(null);
        tile.setType(
                isFallbackColorDefault ? TileVisualType.ICON_DEFAULT : TileVisualType.ICON_COLOR);
    }

    private @LayoutRes int getTileLayoutResId() {
        switch (mStyle) {
            case TileStyle.MODERN:
                return R.layout.suggestions_tile_view;
            case TileStyle.MODERN_CONDENSED:
                return R.layout.suggestions_tile_view_condensed;
        }
        assert false;
        return 0;
    }

    private @DimenRes int getTileWidthDimenResId() {
        switch (mStyle) {
            case TileStyle.MODERN:
                return R.dimen.tile_view_width;
            case TileStyle.MODERN_CONDENSED:
                return R.dimen.tile_view_width_condensed;
        }
        assert false;
        return 0;
    }

    public void setIconGeneratorForTesting(RoundedIconGenerator generator) {
        mIconGenerator = generator;
    }
}
