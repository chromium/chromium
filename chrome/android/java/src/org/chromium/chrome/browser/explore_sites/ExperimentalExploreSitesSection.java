// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.LinearLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/**
 * Describes a portion of UI responsible for rendering a group of categories.
 * It abstracts general tasks related to initializing and fetching data for the UI.
 */
public class ExperimentalExploreSitesSection {
    private static final int MAX_TILES = 3;

    private Profile mProfile;
    private NativePageNavigationDelegate mNavigationDelegate;
    private View mExploreSection;
    private LinearLayout mCategorySection;

    public ExperimentalExploreSitesSection(
            View view, Profile profile, NativePageNavigationDelegate navigationDelegate) {
        mProfile = profile;
        mExploreSection = view;
        mNavigationDelegate = navigationDelegate;
        initialize();
    }

    private void initialize() {
        mCategorySection = mExploreSection.findViewById(R.id.experimental_explore_sites_tiles);
        ExploreSitesBridgeExperimental.getNtpCategories(mProfile, this::initializeTiles);

        View moreCategoriesButton =
                mExploreSection.findViewById(R.id.experimental_explore_sites_more_button);
        moreCategoriesButton.setOnClickListener(
                (View v)
                        -> mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                                new LoadUrlParams(
                                        ExploreSitesBridgeExperimental.nativeGetCatalogUrl(),
                                        PageTransition.AUTO_BOOKMARK)));
    }

    private void initializeTiles(List<ExploreSitesCategoryTile> tileList) {
        if (tileList == null) return;

        // TODO(chili): Try to get this from view hierarchy. This gets called before the
        // mExploreSection is measured when opening ntp via 3 dot menu -> new tab,
        // causing a crash. Max width is set to tile grid max width.
        Point screenSize = new Point();
        ((WindowManager) mExploreSection.getContext().getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getSize(screenSize);
        int tileWidth = Math.min(screenSize.x,
                                mExploreSection.getResources().getDimensionPixelSize(
                                        R.dimen.tile_grid_layout_max_width))
                / MAX_TILES;

        int tileCount = 0;
        for (final ExploreSitesCategoryTile tile : tileList) {
            // Ensures only 3 tiles are shown.
            tileCount++;
            if (tileCount > MAX_TILES) break;

            final ExperimentalExploreSitesCategoryTileView tileView =
                    (ExperimentalExploreSitesCategoryTileView) LayoutInflater
                            .from(mExploreSection.getContext())
                            .inflate(R.layout.experimental_explore_sites_category_tile_view,
                                    mCategorySection, false);

            tileView.initialize(tile, tileWidth);
            mCategorySection.addView(tileView);
            tileView.setOnClickListener(
                    (View v)
                            -> mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                                    new LoadUrlParams(tile.getNavigationUrl(),
                                            PageTransition.AUTO_BOOKMARK)));
            ExploreSitesBridgeExperimental.getIcon(
                    mProfile, tile.getIconUrl(), (Bitmap icon) -> onIconRetrieved(tileView, icon));
        }
    }

    private void onIconRetrieved(ExperimentalExploreSitesCategoryTileView tileView, Bitmap icon) {
        tileView.updateIcon(icon);
    }
}
