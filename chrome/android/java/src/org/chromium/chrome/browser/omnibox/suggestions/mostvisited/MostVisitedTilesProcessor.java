// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.browser.suggestions.tile.TileView;
import org.chromium.chrome.browser.suggestions.tile.TileViewBinder;
import org.chromium.chrome.browser.suggestions.tile.TileViewProperties;
import org.chromium.chrome.browser.ui.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * SuggestionProcessor for Most Visited URL tiles.
 * TODO(crbug.com/1106109): Write integration tests.
 */
public class MostVisitedTilesProcessor extends BaseCarouselSuggestionProcessor {
    private final @NonNull Context mContext;
    private final @NonNull SuggestionHost mSuggestionHost;
    private final @NonNull Supplier<LargeIconBridge> mIconBridgeSupplier;
    private final int mMinCarouselItemViewHeight;
    private static final int DEFAULT_TILE_TYPE = 0;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param host SuggestionHost receiving notifications about user actions.
     * @param iconBridgeSupplier Supplier of the LargeIconBridge used to fetch site favicons.
     */
    public MostVisitedTilesProcessor(@NonNull Context context, @NonNull SuggestionHost host,
            @NonNull Supplier<LargeIconBridge> iconBridgeSupplier) {
        super(context);
        mContext = context;
        mSuggestionHost = host;
        mIconBridgeSupplier = iconBridgeSupplier;
        mMinCarouselItemViewHeight =
                context.getResources().getDimensionPixelSize(R.dimen.tile_view_min_height);
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion, int position) {
        return suggestion.getType() == OmniboxSuggestionType.TILE_NAVSUGGEST;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.TILE_NAVSUGGEST;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public int getMinimumCarouselItemViewHeight() {
        return mMinCarouselItemViewHeight;
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        final List<OmniboxSuggestion.NavsuggestTile> tiles = suggestion.getNavsuggestTiles();
        final int tilesCount = tiles.size();
        final List<ListItem> tileList = new ArrayList<>(tilesCount);
        final LargeIconBridge iconBridge = mIconBridgeSupplier.get();

        for (int index = 0; index < tilesCount; index++) {
            final PropertyModel tileModel = new PropertyModel(TileViewProperties.ALL_KEYS);
            final GURL url = tiles.get(index).url;
            tileModel.set(TileViewProperties.TITLE, tiles.get(index).title);
            tileModel.set(TileViewProperties.TITLE_LINES, 1);
            tileModel.set(TileViewProperties.ON_FOCUS_VIA_SELECTION,
                    () -> { mSuggestionHost.setOmniboxEditingText(url.getSpec()); });

            if (iconBridge != null) {
                iconBridge.getLargeIconForUrl(tiles.get(index).url, /* size */ 32,
                        (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                                int iconType) -> {
                            if (icon == null) return;
                            tileModel.set(TileViewProperties.ICON, new BitmapDrawable(icon));
                        });
            }

            tileList.add(new ListItem(DEFAULT_TILE_TYPE, tileModel));
        }

        model.set(BaseCarouselSuggestionViewProperties.TILES, tileList);
        model.set(BaseCarouselSuggestionViewProperties.TITLE,
                mContext.getResources().getString(R.string.most_visited_tiles_header));
    }

    /**
     * Create Carousel Suggestion View presenting the Most Visited URL tiles.
     *
     * @param parent ViewGroup that will host the Carousel view.
     * @return BaseCarouselSuggestionView for the Most Visited URL suggestions.
     */
    public static BaseCarouselSuggestionView createView(ViewGroup parent) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(new ModelList());
        adapter.registerType(
                DEFAULT_TILE_TYPE, MostVisitedTilesProcessor::buildTile, TileViewBinder::bind);
        return new BaseCarouselSuggestionView(parent.getContext(), adapter);
    }

    /**
     * Create a Tile element for the Most Visited URL suggestions.
     *
     * @param parent ViewGroup that will host the Tile.
     * @return A TileView element for the individual URL suggestion.
     */
    private static TileView buildTile(ViewGroup parent) {
        TileView tile = (TileView) LayoutInflater.from(parent.getContext())
                                .inflate(R.layout.suggestions_tile_view, parent, false);
        tile.setClickable(true);

        Drawable background = OmniboxResourceProvider.resolveAttributeToDrawable(
                parent.getContext(), OmniboxTheme.LIGHT_THEME, R.attr.selectableItemBackground);
        tile.setBackgroundDrawable(background);
        return tile;
    }
}
