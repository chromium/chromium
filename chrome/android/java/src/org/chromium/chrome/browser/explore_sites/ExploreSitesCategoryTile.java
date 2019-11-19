// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.drawable.Drawable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/**
 * Class encapsulating data needed to render a category tile for explore sites section
 * on the NTP.
 */
@JNINamespace("explore_sites")
public class ExploreSitesCategoryTile {
    private String mNavigationUrl;
    private String mIconUrl;
    private String mCategoryName;
    private Drawable mIconDrawable;

    public ExploreSitesCategoryTile() {
        mNavigationUrl = "";
        mIconUrl = "";
        mCategoryName = "";
    }

    public ExploreSitesCategoryTile(String categoryName, String iconUrl, String navigationUrl) {
        mCategoryName = categoryName;
        mIconUrl = iconUrl;
        mNavigationUrl = ExploreSitesBridgeExperimentalJni.get().getCatalogUrl() + navigationUrl;
    }

    public String getNavigationUrl() {
        return mNavigationUrl;
    }

    public String getIconUrl() {
        return mIconUrl;
    }

    public String getCategoryName() {
        return mCategoryName;
    }

    public void setIconDrawable(Drawable iconDrawable) {
        mIconDrawable = iconDrawable;
    }

    public Drawable getIconDrawable() {
        return mIconDrawable;
    }

    @CalledByNative
    private static void createInList(List<ExploreSitesCategoryTile> resultList,
            String navigationUrl, String iconUrl, String categoryName) {
        resultList.add(new ExploreSitesCategoryTile(categoryName, iconUrl, navigationUrl));
    }
}
