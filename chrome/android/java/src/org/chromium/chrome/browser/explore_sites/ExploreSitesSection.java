// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCategory.CategoryType;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.TileGridLayout;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Describes a portion of UI responsible for rendering a group of categories.
 * It abstracts general tasks related to initializing and fetching data for the UI.
 */
public class ExploreSitesSection {
    private static final String TAG = "ExploreSitesSection";
    private static final int MAX_CATEGORIES = 3;

    @TileStyle
    private int mStyle;
    private Profile mProfile;
    private NativePageNavigationDelegate mNavigationDelegate;
    private TileGridLayout mExploreSection;

    public ExploreSitesSection(View view, Profile profile,
            NativePageNavigationDelegate navigationDelegate, @TileStyle int style) {
        mProfile = profile;
        mStyle = style;
        mExploreSection = (TileGridLayout) view;
        mExploreSection.setMaxRows(1);
        mExploreSection.setMaxColumns(MAX_CATEGORIES + 1);
        mNavigationDelegate = navigationDelegate;
        initialize();
    }

    private void initialize() {
        RecordUserAction.record("Android.ExploreSitesNTP.Opened");
        ExploreSitesBridge.getEspCatalog(mProfile, this::gotEspCatalog);
    }

    private Drawable getVectorDrawable(int resource) {
        return VectorDrawableCompat.create(
                getContext().getResources(), resource, getContext().getTheme());
    }

    private Context getContext() {
        return mExploreSection.getContext();
    }

    /**
     * Creates the predetermined categories for when we don't yet have a catalog from the
     * ExploreSites API server.
     */
    private List<ExploreSitesCategory> createDefaultCategoryTiles() {
        List<ExploreSitesCategory> categoryList = new ArrayList<>();

        // News category.
        ExploreSitesCategory category =
                new ExploreSitesCategory(-1 /* category_id */, CategoryType.NEWS,
                        getContext().getString(R.string.explore_sites_default_category_news));
        category.setDrawable(getVectorDrawable(R.drawable.ic_article_blue_24dp));
        categoryList.add(category);

        // Shopping category.
        category = new ExploreSitesCategory(-1 /* category_id */, CategoryType.SHOPPING,
                getContext().getString(R.string.explore_sites_default_category_shopping));
        category.setDrawable(getVectorDrawable(R.drawable.ic_shopping_basket_blue_24dp));
        categoryList.add(category);

        // Sport category.
        category = new ExploreSitesCategory(-1 /* category_id */, CategoryType.SPORT,
                getContext().getString(R.string.explore_sites_default_category_sports));
        category.setDrawable(getVectorDrawable(R.drawable.ic_directions_run_blue_24dp));
        categoryList.add(category);

        return categoryList;
    }

    private ExploreSitesCategory createMoreTileCategory() {
        ExploreSitesCategory category = new ExploreSitesCategory(-1 /* category_id */,
                ExploreSitesCategory.MORE_BUTTON_ID, getContext().getString(R.string.more));
        category.setDrawable(getVectorDrawable(R.drawable.ic_arrow_forward_blue_24dp));
        return category;
    }

    private void createTileView(int tileIndex, ExploreSitesCategory category) {
        ExploreSitesCategoryTileView tileView;
        if (mStyle == TileStyle.MODERN_CONDENSED) {
            tileView = (ExploreSitesCategoryTileView) LayoutInflater.from(getContext())
                               .inflate(R.layout.explore_sites_category_tile_view_condensed,
                                       mExploreSection, false);
        } else {
            tileView = (ExploreSitesCategoryTileView) LayoutInflater.from(getContext())
                               .inflate(R.layout.explore_sites_category_tile_view, mExploreSection,
                                       false);
        }
        tileView.initialize(category);
        mExploreSection.addView(tileView);
        tileView.setOnClickListener((View v) -> onClicked(tileIndex, category, v));
    }

    /**
     * Checks the result, if it indicates that we don't have a valid catalog, request one from the
     * network.  If the network request fails, just continue but otherwise retry getting the catalog
     * from the ExploreSitesBridge.
     */
    private void gotEspCatalog(List<ExploreSitesCategory> categoryList) {
        boolean loadingCatalogFromNetwork = false;
        if (categoryList == null || categoryList.size() == 0) {
            loadingCatalogFromNetwork = true;
            ExploreSitesBridge.updateCatalogFromNetwork(mProfile, true /*isImmediateFetch*/,
                    (Boolean success) -> { updateCategoryIcons(); });
        }
        RecordHistogram.recordBooleanHistogram(
                "ExploreSites.NTPLoadingCatalogFromNetwork", loadingCatalogFromNetwork);
        // Initialize with defaults right away.
        initializeCategoryTiles(categoryList);
    }

    private void updateCategoryIcons() {
        Map<Integer, ExploreSitesCategoryTileView> viewTypes = new HashMap<>();
        for (int i = 0; i < mExploreSection.getChildCount(); i++) {
            ExploreSitesCategoryTileView v =
                    (ExploreSitesCategoryTileView) mExploreSection.getChildAt(i);
            ExploreSitesCategory category = v.getCategory();
            if (category == null || category.getType() == ExploreSitesCategory.MORE_BUTTON_ID)
                continue;
            viewTypes.put(category.getType(), v);
        }

        ExploreSitesBridge.getEspCatalog(mProfile, (List<ExploreSitesCategory> categoryList) -> {
            for (ExploreSitesCategory category : categoryList) {
                ExploreSitesCategoryTileView v = viewTypes.get(category.getType());
                if (v == null) {
                    continue;
                }
                int iconSizePx = v.getContext().getResources().getDimensionPixelSize(
                        R.dimen.tile_view_icon_size);
                ExploreSitesBridge.getCategoryImage(
                        mProfile, category.getId(), iconSizePx, (Bitmap image) -> {
                            if (image != null) {
                                category.setDrawable(ViewUtils.createRoundedBitmapDrawable(
                                        image, iconSizePx / 2));
                                v.renderIcon(category);
                            }
                        });
            }
        });
    }

    private void initializeCategoryTiles(List<ExploreSitesCategory> categoryList) {
        boolean needIcons = true;
        if (categoryList == null || categoryList.size() == 0) {
            categoryList = createDefaultCategoryTiles();
            needIcons = false; // Icons are already prepared in the default tiles.
        }

        int tileCount = 0;
        for (final ExploreSitesCategory category : categoryList) {
            if (tileCount >= MAX_CATEGORIES) break;
            createTileView(tileCount, category);
            tileCount++;
        }
        createTileView(tileCount, createMoreTileCategory());
        if (needIcons) {
            updateCategoryIcons();
        }
    }

    private void onClicked(int tileIndex, ExploreSitesCategory category, View v) {
        RecordHistogram.recordLinearCountHistogram(
                "ExploreSites.ClickedNTPCategoryIndex", tileIndex, 1, 100, 100);
        mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(category.getUrl(), PageTransition.AUTO_BOOKMARK));
    }
}
