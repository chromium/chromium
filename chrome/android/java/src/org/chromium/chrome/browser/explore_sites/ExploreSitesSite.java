// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.modelutil.PropertyModel;

/**
 * An object encapsulating info for a website.
 */
public class ExploreSitesSite {
    static final PropertyModel.ReadableIntPropertyKey ID_KEY =
            new PropertyModel.ReadableIntPropertyKey();
    static final PropertyModel.WritableIntPropertyKey TILE_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.ReadableObjectPropertyKey<String> TITLE_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.ReadableObjectPropertyKey<String> URL_KEY =
            new PropertyModel.ReadableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<Bitmap> ICON_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();

    private PropertyModel mModel;

    public ExploreSitesSite(int id, int tileIndex, String title, String url) {
        mModel = new PropertyModel.Builder(ID_KEY, TILE_INDEX_KEY, TITLE_KEY, URL_KEY, ICON_KEY)
                         .with(ID_KEY, id)
                         .with(TILE_INDEX_KEY, tileIndex)
                         .with(TITLE_KEY, title)
                         .with(URL_KEY, url)
                         .build();
    }

    public PropertyModel getModel() {
        return mModel;
    }

    @CalledByNative
    private static void createSiteInCategory(
            int siteId, String title, String url, ExploreSitesCategory category) {
        ExploreSitesSite site =
                new ExploreSitesSite(siteId, category.getSites().size(), title, url);
        category.addSite(site);
    }
}
