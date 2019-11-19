// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.tile.TileGridLayout;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.ArrayList;
import java.util.List;

/**
 * View for a category name and site tiles.
 */
public class ExploreSitesCategoryCardView extends LinearLayout {
    private static final String TAG = "ExploreSitesCategoryCardView";

    private final ExploreSitesSiteViewBinder mSiteViewBinder;
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
    private int mTileViewLayout;
    private boolean mIsDense;
    private int mMaxRows;
    private int mMaxColumns;
    private int mMaxTileCount;

    public View getTileViewAt(int tilePosition) {
        return mTileView.getChildAt(tilePosition);
    }

    public int getFocusedTileIndex(int defaultIndex) {
        if (mTileView.getFocusedChild() != null) {
            for (int i = 0; i < mTileView.getChildCount(); i++) {
                if (mTileView.getChildAt(i).hasFocus()) {
                    return i;
                }
            }
        }
        return defaultIndex;
    }

    public void setTileResource(int tileResource) {
        mTileViewLayout = tileResource;
    }

    protected class CategoryCardInteractionDelegate
            implements ContextMenuManager.Delegate, OnClickListener, OnCreateContextMenuListener,
                       OnFocusChangeListener {
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
            RecordUserAction.record("Android.ExploreSitesPage.ClickOnSiteIcon");
            ExploreSitesBridge.recordClick(mProfile, mSiteUrl, mCategory.getType());
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

            // Update the view. This may add sites that we didn't have room for before.  It
            // should reset the tile indexes for views we keep.
            updateTileViews(mCategory);
        }

        @Override
        public String getUrl() {
            return mSiteUrl;
        }

        @Override
        public String getContextMenuTitle() {
            return null;
        }

        @Override
        public boolean isItemSupported(@ContextMenuManager.ContextMenuItemId int menuItemId) {
            return menuItemId != ContextMenuManager.ContextMenuItemId.LEARN_MORE;
        }

        @Override
        public void onContextMenuCreated() {}

        @Override
        public void onFocusChange(View v, boolean hasFocus) {
            if (hasFocus) {
                // Ensures the whole category card is scrolled to view when a child site has focus.
                // Immediate should be false so scrolling will not interfere with any existing
                // scrollers running to make the view visible.
                getParent().requestChildRectangleOnScreen(ExploreSitesCategoryCardView.this,
                        new Rect(/* left= */ 0, /* top= */ 0, /* right= */ getWidth(),
                                /* bottom= */ getHeight()),
                        /* immediate= */ false);
            }
        }
    }

    protected CategoryCardInteractionDelegate createInteractionDelegate(PropertyModel model) {
        return new CategoryCardInteractionDelegate(
                model.get(ExploreSitesSite.URL_KEY), model.get(ExploreSitesSite.TILE_INDEX_KEY));
    }

    // We use the MVC paradigm for the site tiles inside the category card.  We don't use the MVC
    // paradigm for the category card view itself since it is mismatched to the needs of the
    // recycler view that we use for category cards.  The controller for MVC is actually here, the
    // bind code inside the view class.
    protected class ExploreSitesSiteViewBinder
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
                        createInteractionDelegate(model);
                view.setOnClickListener(interactionDelegate);
                view.setOnCreateContextMenuListener(interactionDelegate);
                view.setOnFocusChangeListener(interactionDelegate);
            }
        }
    }

    public ExploreSitesCategoryCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mModelChangeProcessors = new ArrayList<>();
        mSiteViewBinder = new ExploreSitesSiteViewBinder();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.category_title);
        mTileView = findViewById(R.id.category_sites);
    }

    public void setTileGridParams(int maxRows, int maxColumns, @DenseVariation int denseVariation) {
        mIsDense = ExploreSitesBridge.isDense(denseVariation);
        mMaxRows = maxRows;
        mMaxColumns = maxColumns;
        mMaxTileCount = mMaxRows * mMaxColumns;
        mModelChangeProcessors.clear();
        mModelChangeProcessors = new ArrayList<>(mMaxTileCount);

        mTileView.setMaxColumns(mMaxColumns);
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

        updateTitle(mCategory.getTitle());
        updateTileViews(mCategory);
    }

    public void updateTitle(String categoryTitle) {
        mTitleView.setText(categoryTitle);
    }

    public void updateTileViews(ExploreSitesCategory category) {
        // Clear observers.
        for (PropertyModelChangeProcessor<PropertyModel, ExploreSitesTileView, PropertyKey>
                        observer : mModelChangeProcessors) {
            observer.destroy();
        }
        mModelChangeProcessors.clear();

        boolean incompleteAllowed = allowIncompleteRow(category);
        int tileMax = tilesToDisplay(category, incompleteAllowed);
        mTileView.setMaxRows(rowsToDisplay(category, incompleteAllowed));

        // Remove extra tiles if too many.
        if (mTileView.getChildCount() > tileMax) {
            mTileView.removeViews(tileMax, mTileView.getChildCount() - tileMax);
        }

        // Add tiles if too few
        if (mTileView.getChildCount() < tileMax) {
            for (int i = mTileView.getChildCount(); i < tileMax; i++) {
                mTileView.addView(LayoutInflater.from(getContext())
                                          .inflate(mTileViewLayout, mTileView,
                                                  /* attachToRoot = */ false));
            }
        }

        // Initialize all the non-empty tiles again to update.
        int tileIndex = 0;
        for (ExploreSitesSite site : category.getSites()) {
            if (tileIndex >= tileMax) break;
            final PropertyModel siteModel = site.getModel();
            // Skip blacklisted sites.
            if (siteModel.get(ExploreSitesSite.BLACKLISTED_KEY)) continue;

            ExploreSitesTileView tileView = (ExploreSitesTileView) mTileView.getChildAt(tileIndex);
            tileView.initialize(mIconGenerator);

            siteModel.set(ExploreSitesSite.TILE_INDEX_KEY, tileIndex);

            mModelChangeProcessors.add(
                    PropertyModelChangeProcessor.create(siteModel, tileView, mSiteViewBinder));

            // Fetch icon if not present already.
            if (siteModel.get(ExploreSitesSite.ICON_KEY) == null) {
                ExploreSitesBridge.getSiteImage(mProfile, siteModel.get(ExploreSitesSite.ID_KEY),
                        (Bitmap icon) -> siteModel.set(ExploreSitesSite.ICON_KEY, icon));
            }
            tileIndex++;
        }
    }

    /**
     * Records UMA data for which category when the user clicks a tile in that category.
     * @param category The category the user picked.
     */
    public static void recordCategoryClick(int category) {
        RecordHistogram.recordEnumeratedHistogram("ExploreSites.CategoryClick", category,
                ExploreSitesCategory.CategoryType.NUM_ENTRIES);
    }

    /**
     * Records UMA data for how far down the EoS page the picked tile was.
     * @param cardIndex The number card (zero based) of the tile that was picked.
     * @param tileIndex The number of the tile within the card.
     */
    public void recordTileIndexClick(int cardIndex, int tileIndex) {
        // TODO(petewil): Should I get the number of sites in this category from the model instead
        // of using MAX_TILE_COUNT?
        RecordHistogram.recordLinearCountHistogram("ExploreSites.SiteTilesClickIndex2",
                cardIndex * ExploreSitesPage.MAX_TILE_COUNT_ALL_VARIATIONS + tileIndex, 1, 100,
                100);
    }

    /**
     * Determine if an incomplete row will be allowed when the view is dense.
     *
     * An incomplete row is not allowed regardless of the below constraints if:
     *  - The view is not dense.
     *  - There are more sites to display than mMaxTileCount.
     *  - The last row forms a complete row of sites.
     *
     * An incomplete row is allowed if any of the following constraints are satisfied:
     *  - There are not enough sites to populate the first row.
     *  - There is more than one site in the last row.
     *  - There is one site in the last row as a result of the user blacklisting a site.
     *
     * @param category The category from which the number of incomplete row will be calculated.
     */
    @VisibleForTesting
    boolean allowIncompleteRow(ExploreSitesCategory category) {
        if (!mIsDense) return false;
        // Do not allow incomplete row if category has more sites than mMaxTileCount.
        if (category.getNumDisplayed() > mMaxTileCount) return false;

        final int numSitesLastRow = category.getNumDisplayed() % mMaxColumns;
        // Do not allow incomplete row if last row forms a complete row anyway.
        if (numSitesLastRow == 0) return false;

        // Allow incomplete row if category does not have enough sites to populate first row.
        if (category.getNumDisplayed() < mMaxColumns) return true;

        return (category.getNumberRemoved() > 0 || numSitesLastRow > 1);
    }

    @VisibleForTesting
    int rowsToDisplay(ExploreSitesCategory category, boolean incompleteAllowed) {
        if (mIsDense) {
            int displayedRows = category.getNumDisplayed() / mMaxColumns;
            return Math.min(displayedRows + (incompleteAllowed ? 1 : 0), mMaxRows);
        } else {
            return Math.min(category.getMaxRows(mMaxColumns), mMaxRows);
        }
    }

    @VisibleForTesting
    int tilesToDisplay(ExploreSitesCategory category, boolean incompleteAllowed) {
        return incompleteAllowed ? Math.min(category.getNumDisplayed(), mMaxTileCount)
                                 : Math.min(Math.min(category.getMaxRows(mMaxColumns) * mMaxColumns,
                                                    category.getNumDisplayed()),
                                         mMaxTileCount);
    }
}
