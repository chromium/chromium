// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * An object encapsulating info for a website.
 */
public class ExploreSitesSite {
    static final int DEFAULT_TILE_INDEX = -1;
    static final PropertyModel.ReadableIntPropertyKey ID_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    public static final PropertyModel.WritableIntPropertyKey TILE_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.ReadableObjectPropertyKey<String> TITLE_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.ReadableObjectPropertyKey<GURL> URL_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<Bitmap> ICON_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableBooleanPropertyKey BLOCKED_KEY =
            new PropertyModel.WritableBooleanPropertyKey();

    private PropertyModel mModel;

    public ExploreSitesSite(int id, String title, GURL url, boolean isBlocked) {
        mModel = new PropertyModel
                         .Builder(ID_KEY, TILE_INDEX_KEY, TITLE_KEY, URL_KEY, ICON_KEY, BLOCKED_KEY)
                         .with(ID_KEY, id)
                         .with(TITLE_KEY, title)
                         .with(URL_KEY, url)
                         .with(BLOCKED_KEY, isBlocked)
                         .with(TILE_INDEX_KEY, DEFAULT_TILE_INDEX)
                         .build();
    }

    public PropertyModel getModel() {
        return mModel;
    }

    @CalledByNative
    private static void createSiteInCategory(
            int siteId, String title, GURL url, boolean isBlocked, ExploreSitesCategory category) {
        ExploreSitesSite site = new ExploreSitesSite(siteId, title, url, isBlocked);
        category.addSite(site);
    }
}
