// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * An object representing a category in Explore Sites.
 */
public class ExploreSitesCategory {
    private static final String TAG = "ExploreSitesCategory";
    // The ID to use when creating the More button, that should not scroll the ESP when clicked.
    public static final int MORE_BUTTON_ID = -1;

    // This enum must match the numbering for ExploreSites.CategoryClick in histograms.xml.  Do not
    // reorder or remove items, only add new items before COUNT.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({CategoryType.DEFAULT, CategoryType.SOCIAL, CategoryType.ENTERTAINMENT,
            CategoryType.SPORT, CategoryType.NEWS, CategoryType.SHOPPING, CategoryType.REFERENCE,
            CategoryType.BANKING, CategoryType.GOVERNMENT, CategoryType.TRAVEL,
            CategoryType.EDUCATION, CategoryType.JOBS, CategoryType.APPS_GAMES,
            CategoryType.FAVORITE, CategoryType.GOOGLE, CategoryType.FOOD, CategoryType.HEALTH,
            CategoryType.BOOKS, CategoryType.TECHNOLOGY, CategoryType.SCIENCE, CategoryType.COUNT})
    public @interface CategoryType {
        int DEFAULT = 0;
        int SOCIAL = 1;
        int ENTERTAINMENT = 2;
        int SPORT = 3;
        int NEWS = 4;
        int SHOPPING = 5;
        int REFERENCE = 6;
        int BANKING = 7;
        int GOVERNMENT = 8;
        int TRAVEL = 9;
        int EDUCATION = 10;
        int JOBS = 11;
        int APPS_GAMES = 12;
        int FAVORITE = 13;
        int GOOGLE = 14;
        int FOOD = 15;
        int HEALTH = 16;
        int BOOKS = 17;
        int TECHNOLOGY = 18;
        int SCIENCE = 19;
        // This must always be one higher than the last category number.
        int COUNT = 20;
    }

    private int mCategoryId;

    private @CategoryType int mCategoryType;
    private String mCategoryTitle;

    // Populated only in NTP.
    private Drawable mDrawable;
    // Populated only for ESP.
    private List<ExploreSitesSite> mSites;

    /**
     * Creates an explore sites category data structure.
     * @param categoryId The integer category ID, corresponding to the row in the DB this reflects.
     * @param categoryType The integer category type, corresponding to the enum value from the
     *         Catalog proto, or -1 if this represents the More button.
     * @param title The string to display as the caption for this tile.
     */
    public ExploreSitesCategory(int categoryId, @CategoryType int categoryType, String title) {
        mCategoryId = categoryId;
        mCategoryType = categoryType;
        mCategoryTitle = title;
        mSites = new ArrayList<>();
    }

    public int getId() {
        return mCategoryId;
    }
    public @CategoryType int getType() {
        return mCategoryType;
    }

    public boolean isPlaceholder() {
        return mCategoryId == -1;
    }

    public String getTitle() {
        return mCategoryTitle;
    }

    public void setIcon(Context context, Bitmap icon) {
        mDrawable = new BitmapDrawable(context.getResources(), icon);
    }

    public void setDrawable(Drawable drawable) {
        mDrawable = drawable;
    }

    public Drawable getDrawable() {
        return mDrawable;
    }

    public void addSite(ExploreSitesSite site) {
        mSites.add(site);
    }

    public boolean removeSite(int siteIndex) {
        if (siteIndex > mSites.size() || siteIndex < 0) return false;
        mSites.remove(siteIndex);

        // Reset the tile indices to account for removed tiles.
        for (int i = siteIndex; i < mSites.size(); ++i) {
            ExploreSitesSite site = mSites.get(i);
            site.getModel().set(ExploreSitesSite.TILE_INDEX_KEY, i);
        }
        return true;
    }

    public List<ExploreSitesSite> getSites() {
        return mSites;
    }

    /**
     * Returns the URL for the explore sites page, with the correct scrolling ID in the hash value.
     */
    public String getUrl() {
        if (mCategoryId < 0) {
            return UrlConstants.EXPLORE_URL;
        }
        return UrlConstants.EXPLORE_URL + "#" + getId();
    }

    // Creates a new category and appends to the given list. Also returns the created category to
    // easily append sites to the category.
    @CalledByNative
    private static ExploreSitesCategory createAndAppendToList(
            int categoryId, int categoryType, String title, List<ExploreSitesCategory> list) {
        ExploreSitesCategory category = new ExploreSitesCategory(categoryId, categoryType, title);
        list.add(category);
        return category;
    }
}
