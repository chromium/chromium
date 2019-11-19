// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.util.UrlConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * An object representing a category in Explore Sites.
 */
public class ExploreSitesCategory {
    private static final String TAG = "ExploreSitesCategory";
    // The ID to use when creating placeholder icons on the NTP when there is no data or the More
    // button.
    private static final int PLACEHOLDER_ID = -1;

    // This enum must match the numbering for ExploreSites.CategoryClick in histograms.xml.  Do not
    // reorder or remove items, only add new items before NUM_ENTRIES.
    @IntDef({CategoryType.MORE_BUTTON, CategoryType.DEFAULT, CategoryType.SOCIAL,
            CategoryType.ENTERTAINMENT, CategoryType.SPORT, CategoryType.NEWS,
            CategoryType.SHOPPING, CategoryType.REFERENCE, CategoryType.BANKING,
            CategoryType.GOVERNMENT, CategoryType.TRAVEL, CategoryType.EDUCATION, CategoryType.JOBS,
            CategoryType.APPS_GAMES, CategoryType.FAVORITE, CategoryType.GOOGLE, CategoryType.FOOD,
            CategoryType.HEALTH, CategoryType.BOOKS, CategoryType.TECHNOLOGY, CategoryType.SCIENCE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CategoryType {
        int MORE_BUTTON = -1; // This is not included in histograms.xml.
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
        int NUM_ENTRIES = 20;
    }

    public static ExploreSitesCategory createPlaceholder(
            @CategoryType int categoryType, String title) {
        return new ExploreSitesCategory(PLACEHOLDER_ID, categoryType, title, 0, 0);
    }

    private int mCategoryId;

    private @CategoryType int mCategoryType;
    private String mCategoryTitle;

    // Populated only in NTP.
    private Drawable mDrawable;
    private int mNtpShownCount;
    private int mInteractionCount;
    // Populated only for ESP.
    private List<ExploreSitesSite> mSites;
    private int mNumRemoved;

    /**
     * Creates an explore sites category data structure.
     * @param categoryId The integer category ID, corresponding to the row in the DB this reflects.
     * @param categoryType The integer category type, corresponding to the enum value from the
     *         Catalog proto, or -1 if this represents the More button.
     * @param title The string to display as the caption for this tile.
     */
    public ExploreSitesCategory(int categoryId, @CategoryType int categoryType, String title,
            int ntpShownCount, int interactionCount) {
        mCategoryId = categoryId;
        mCategoryType = categoryType;
        mCategoryTitle = title;
        mNtpShownCount = ntpShownCount;
        mInteractionCount = interactionCount;
        mSites = new ArrayList<>();
    }

    public int getId() {
        return mCategoryId;
    }
    public @CategoryType int getType() {
        return mCategoryType;
    }

    public boolean isPlaceholder() {
        return mCategoryId == PLACEHOLDER_ID;
    }

    public boolean isMoreButton() {
        return mCategoryType == CategoryType.MORE_BUTTON;
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

    public int getNtpShownCount() {
        return mNtpShownCount;
    }

    public int getInteractionCount() {
        return mInteractionCount;
    }

    public void addSite(ExploreSitesSite site) {
        mSites.add(site);
        if (site.getModel().get(ExploreSitesSite.BLACKLISTED_KEY)) {
            mNumRemoved++;
        }
    }

    public int getNumDisplayed() {
        return mSites.size() - mNumRemoved;
    }

    /**
     * Get the number of rows that could be filled completely with sites, if no site is blacklisted.
     * @param numColumns - number of columns wide the layout holding this category is. This
     *                   parameter must not be zero.
     */
    public int getMaxRows(int numColumns) {
        return mSites.size() / numColumns;
    }

    public int getNumberRemoved() {
        return mNumRemoved;
    }

    public boolean removeSite(int tileIndex) {
        if (tileIndex > mSites.size() || tileIndex < 0) return false;

        // Find the siteIndex for the tileIndex by skipping over blacklisted sites.
        int siteIndex = 0;
        int validSiteCount = 0;
        while (siteIndex < mSites.size()) {
            // Skipping over blacklisted sites, look for the nth unblacklisted site.
            if (!mSites.get(siteIndex).getModel().get(ExploreSitesSite.BLACKLISTED_KEY)) {
                validSiteCount++;
            }

            // When we find the nth valid site, we have found the site index matching the tile.
            // TileIndex is 0 based, validSiteCount is 1 based, so we add 1 to the tileIndex.
            if (tileIndex + 1 == validSiteCount
                    && !mSites.get(siteIndex).getModel().get(ExploreSitesSite.BLACKLISTED_KEY)) {
                break;
            }

            siteIndex++;
        }
        if (siteIndex >= mSites.size()) return false;

        mSites.get(siteIndex).getModel().set(ExploreSitesSite.BLACKLISTED_KEY, true);

        // Reset the tile indices to account for removed tile.
        mSites.get(siteIndex).getModel().set(
                ExploreSitesSite.TILE_INDEX_KEY, ExploreSitesSite.DEFAULT_TILE_INDEX);

        for (int i = siteIndex; i < mSites.size(); ++i) {
            ExploreSitesSite site = mSites.get(i);
            if (!mSites.get(i).getModel().get(ExploreSitesSite.BLACKLISTED_KEY)) {
                site.getModel().set(ExploreSitesSite.TILE_INDEX_KEY, tileIndex);
                tileIndex++;
            }
        }
        mNumRemoved++;
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
    private static ExploreSitesCategory createAndAppendToList(int categoryId, int categoryType,
            String title, int ntpShownCount, int interactionCount,
            List<ExploreSitesCategory> list) {
        ExploreSitesCategory category = new ExploreSitesCategory(
                categoryId, categoryType, title, ntpShownCount, interactionCount);
        list.add(category);
        return category;
    }
}
