// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnCreateContextMenuListener;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.TileGridLayout;
import org.chromium.chrome.browser.widget.RoundedIconGenerator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.List;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesCategoryCardView extends LinearLayout {
    private static final String TAG = "ExploreSitesCategoryCardView";
    private static final int MAX_TILE_COUNT = 8;
    private static final int MAX_COLUMNS = 4;
    private static final int MAX_ROWS = 2;

    private TextView mTitleView;
    private TileGridLayout mTileView;
    private RoundedIconGenerator mIconGenerator;
    private ContextMenuManager mContextMenuManager;
    private NativePageNavigationDelegate mNavigationDelegate;
    private Profile mProfile;
    private List<PropertyModelChangeProcessor<PropertyModel, ExploreSitesTileView, PropertyKey>>
            mModelChangeProcessors;
    private ExploreSitesCategory mCategory;
    private int mCategoryCardIndex;

    private class CategoryCardInteractionDelegate
            implements ContextMenuManager.Delegate, OnClickListener, OnCreateContextMenuListener {
        private String mSiteUrl;
        private int mTileIndex;

        public CategoryCardInteractionDelegate(String siteUrl, int tileIndex) {
            mSiteUrl = siteUrl;
            mTileIndex = tileIndex;
        }

        @Override
        public void onClick(View view) {
            recordCategoryClick(mCategory.getType());
            recordTileIndexClick(mCategoryCardIndex, mTileIndex);
            mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                    new LoadUrlParams(getUrl(), PageTransition.AUTO_BOOKMARK));
        }

        @Override
        public void onCreateContextMenu(
                ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
            mContextMenuManager.createContextMenu(menu, v, this);
        }

        @Override
        public void openItem(int windowDisposition) {
            mNavigationDelegate.openUrl(
                    windowDisposition, new LoadUrlParams(getUrl(), PageTransition.AUTO_BOOKMARK));
        }

        @Override
        public void removeItem() {
            // Update the database on the C++ side.
            ExploreSitesBridge.blacklistSite(mProfile, mSiteUrl);

            // Remove from model (category).
            mCategory.removeSite(mTileIndex);

            // Update the view This may add any sites that we didn't have room for before.  It
            // should reset the tile indexeds for views we keep.
            updateTileViews(mCategory.getSites());
        }
        @Override
        public String getUrl() {
            return mSiteUrl;
        }

        @Override
        public boolean isItemSupported(@ContextMenuManager.ContextMenuItemId int menuItemId) {
            if (menuItemId == ContextMenuManager.ContextMenuItemId.LEARN_MORE) {
                return false;
            }
            return true;
        }

        @Override
        public void onContextMenuCreated(){};
    }

    // We use the MVC paradigm for the site tiles inside the category card.  We don't use the MVC
    // paradigm for the category card view itself since it is mismatched to the needs of the
    // recycler view that we use for category cards.  The controller for MVC is actually here, the
    // bind code inside the view class.
    private class ExploreSitesSiteViewBinder
            implements PropertyModelChangeProcessor
                               .ViewBinder<PropertyModel, ExploreSitesTileView, PropertyKey> {
        @Override
        public void bind(PropertyModel model, ExploreSitesTileView view, PropertyKey key) {
            if (key == ExploreSitesSite.ICON_KEY) {
                view.updateIcon(model.get(ExploreSitesSite.ICON_KEY),
                        model.get(ExploreSitesSite.TITLE_KEY));
            } else if (key == ExploreSitesSite.TITLE_KEY) {
                view.setTitle(model.get(ExploreSitesSite.TITLE_KEY));
            } else if (key == ExploreSitesSite.URL_KEY) {
                // Attach click handlers.
                CategoryCardInteractionDelegate interactionDelegate =
                        new CategoryCardInteractionDelegate(model.get(ExploreSitesSite.URL_KEY),
                                model.get(ExploreSitesSite.TILE_INDEX_KEY));
                view.setOnClickListener(interactionDelegate);
                view.setOnCreateContextMenuListener(interactionDelegate);
            }
        }
    }

    public ExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mModelChangeProcessors = new ArrayList<>(MAX_TILE_COUNT);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.category_title);
        mTileView = findViewById(R.id.category_sites);
        mTileView.setMaxColumns(MAX_COLUMNS);
        mTileView.setMaxRows(MAX_ROWS);
    }

    public void setCategory(ExploreSitesCategory category, int categoryCardIndex,
            RoundedIconGenerator iconGenerator, ContextMenuManager contextMenuManager,
            NativePageNavigationDelegate navigationDelegate, Profile profile) {
        mIconGenerator = iconGenerator;
        mContextMenuManager = contextMenuManager;
        mNavigationDelegate = navigationDelegate;
        mProfile = profile;
        mCategoryCardIndex = categoryCardIndex;
        mCategory = category;

        updateTitle(category.getTitle());
        updateTileViews(category.getSites());
    }

    public void updateTitle(String categoryTitle) {
        mTitleView.setText(categoryTitle);
    }

    public void updateTileViews(List<ExploreSitesSite> sites) {
        // Clear observers.
        for (PropertyModelChangeProcessor<PropertyModel, ExploreSitesTileView, PropertyKey>
                        observer : mModelChangeProcessors) {
            observer.destroy();
        }
        mModelChangeProcessors.clear();

        // Remove extra tiles if too many.
        if (mTileView.getChildCount() > sites.size()) {
            mTileView.removeViews(sites.size(), mTileView.getChildCount() - sites.size());
        }

        // Maximum number of sites to show.
        int tileMax = Math.min(MAX_TILE_COUNT, sites.size());

        // Add tiles if too few
        if (mTileView.getChildCount() < tileMax) {
            for (int i = mTileView.getChildCount(); i < tileMax; i++) {
                mTileView.addView(LayoutInflater.from(getContext())
                                          .inflate(R.layout.explore_sites_tile_view, mTileView,
                                                  /* attachToRoot = */ false));
            }
        }

        // Initialize all the non-empty tiles again to update.
        for (int i = 0; i < tileMax; i++) {
            ExploreSitesTileView tileView = (ExploreSitesTileView) mTileView.getChildAt(i);
            final PropertyModel site = sites.get(i).getModel();
            tileView.initialize(mIconGenerator);

            mModelChangeProcessors.add(PropertyModelChangeProcessor.create(
                    site, tileView, new ExploreSitesSiteViewBinder()));

            // Fetch icon if not present already.
            if (site.get(ExploreSitesSite.ICON_KEY) == null) {
                ExploreSitesBridge.getSiteImage(mProfile, site.get(ExploreSitesSite.ID_KEY),
                        (Bitmap icon) -> site.set(ExploreSitesSite.ICON_KEY, icon));
            }
        }
    }

    /**
     * Records UMA data for which category when the user clicks a tile in that category.
     * @param category The category the user picked.
     */
    public static void recordCategoryClick(int category) {
        RecordHistogram.recordEnumeratedHistogram(
                "ExploreSites.CategoryClick", category, ExploreSitesCategory.CategoryType.COUNT);
    }

    /**
     * Records UMA data for how far down the EoS page the picked tile was.
     * @param cardNumber The number card (zero based) of the tile that was picked.
     * @param tileNumber The number of the tile within the card.
     */
    public static void recordTileIndexClick(int cardIndex, int tileIndex) {
        // TODO(petewil): Should I get the number of sites in this category from the model instead
        // of using MAX_TILE_COUNT?
        RecordHistogram.recordLinearCountHistogram("ExploreSites.SiteTilesClickIndex",
                cardIndex * MAX_TILE_COUNT + tileIndex, 1, 100, 100);
    }
}
