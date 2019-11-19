// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static org.chromium.components.feature_engagement.EventConstants.EXPLORE_SITES_TILE_TAPPED;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCategory.CategoryType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.browser.suggestions.tile.TileGridLayout;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.Collections;
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
    // This is number of times in a row categories are shown in a row before being rotated out.
    private static final int TIMES_PER_ROUND = 3;
    // Max times a category is shown due to rotation.
    private static final int MAX_TIMES_ROTATED = 6;

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
        ExploreSitesCategory category = ExploreSitesCategory.createPlaceholder(CategoryType.NEWS,
                getContext().getString(R.string.explore_sites_default_category_news));
        category.setDrawable(getVectorDrawable(R.drawable.ic_article_blue_24dp));
        categoryList.add(category);

        // Shopping category.
        category = ExploreSitesCategory.createPlaceholder(CategoryType.SHOPPING,
                getContext().getString(R.string.explore_sites_default_category_shopping));
        category.setDrawable(getVectorDrawable(R.drawable.ic_shopping_basket_blue_24dp));
        categoryList.add(category);

        // Sport category.
        category = ExploreSitesCategory.createPlaceholder(CategoryType.SPORT,
                getContext().getString(R.string.explore_sites_default_category_sports));
        category.setDrawable(getVectorDrawable(R.drawable.ic_directions_run_blue_24dp));
        categoryList.add(category);

        return categoryList;
    }

    private ExploreSitesCategory createMoreTileCategory() {
        ExploreSitesCategory category =
                ExploreSitesCategory.createPlaceholder(CategoryType.MORE_BUTTON,
                        getContext().getString(R.string.explore_sites_top_sites_tile));
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
        tileView.initialize(category, mProfile);
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
            RecordHistogram.recordEnumeratedHistogram("ExploreSites.CatalogUpdateRequestSource",
                    ExploreSitesCatalogUpdateRequestSource.NEW_TAB_PAGE,
                    ExploreSitesCatalogUpdateRequestSource.NUM_ENTRIES);
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
            if (category == null || category.isMoreButton()) continue;
            viewTypes.put(category.getType(), v);
        }

        ExploreSitesBridge.getEspCatalog(mProfile, (List<ExploreSitesCategory> categoryList) -> {
            if (categoryList == null) return;
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
                                        v.getContext().getResources(), image, iconSizePx / 2));
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

        boolean isPersonalized =
                ExploreSitesBridge.getVariation() == ExploreSitesVariation.PERSONALIZED;

        if (isPersonalized) {
            // Sort categories in order or shown priority.
            Collections.sort(categoryList, ExploreSitesSection::compareCategoryPriority);
        }

        int tileCount = 0;
        for (final ExploreSitesCategory category : categoryList) {
            if (tileCount >= MAX_CATEGORIES) break;
            // Skip empty categories from being shown on NTP.
            if (!category.isPlaceholder() && category.getNumDisplayed() == 0) continue;
            createTileView(tileCount, category);
            // Increment shown count if this is a category that was rotated in.
            // A rotated in category is defined by having no interaction count
            // and having a shown count less than the MAX_TIMES_ROTATED.
            if (isPersonalized && category.getInteractionCount() == 0
                    && category.getNtpShownCount() < MAX_TIMES_ROTATED
                    && !category.isPlaceholder()) {
                ExploreSitesBridge.incrementNtpShownCount(mProfile, category.getId());
            }
            tileCount++;
        }
        createTileView(tileCount, createMoreTileCategory());
        if (needIcons) {
            updateCategoryIcons();
        }
    }

    private void onClicked(int tileIndex, ExploreSitesCategory category, View v) {
        recordOpenedEsp(tileIndex);
        mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(category.getUrl(), PageTransition.AUTO_BOOKMARK));
        final Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        tracker.notifyEvent(EXPLORE_SITES_TILE_TAPPED);
    }

    private void recordOpenedEsp(int tileIndex) {
        // The following must be kept in sync with the "MostVisitedTileIndex" enum in enums.xml.
        final int kMaxTileCount = 12;
        RecordHistogram.recordEnumeratedHistogram(
                "ExploreSites.ClickedNTPCategoryIndex", tileIndex, kMaxTileCount);
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_EXPLORE_SITES_TILE);
        RecordUserAction.record("MobileNTPExploreSites");
    }

    @VisibleForTesting
    static int compareCategoryPriority(ExploreSitesCategory cat1, ExploreSitesCategory cat2) {
        // First sort by activity count. Most used categories first.
        if (cat1.getInteractionCount() > cat2.getInteractionCount()) return -1;
        if (cat1.getInteractionCount() < cat2.getInteractionCount()) return 1;
        // Category 1 and 2 have the same interaction count. If that is
        // nonzero, they are equal. Collections.sort is stable, which will preserve input order
        // for equal categories. Otherwise we want to rotate the categories.
        if (cat1.getInteractionCount() > 0) return 0;

        // Otherwise activity count is both 0.
        // We first sort by descending ntp_shown_count mod 3 (TIMES_PER_ROUND).
        // This is so categories that haven't completed a round are prioritized.
        int cat1Mod = cat1.getNtpShownCount() % TIMES_PER_ROUND;
        int cat2Mod = cat2.getNtpShownCount() % TIMES_PER_ROUND;
        if (cat1Mod > cat2Mod) return -1;
        if (cat1Mod < cat2Mod) return 1;
        // If the mods are equal, then we sort by ntp_shown_count / 3 in ascending
        // order. This is so categories that haven't been shown yet are prioritized.
        return (cat1.getNtpShownCount() / TIMES_PER_ROUND)
                - (cat2.getNtpShownCount() / TIMES_PER_ROUND);
    }
}
