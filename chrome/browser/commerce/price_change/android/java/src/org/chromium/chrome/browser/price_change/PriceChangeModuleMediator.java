// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;

import android.content.Context;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.Set;

/**
 * Mediator for the price change module which can be embedded by surfaces like NTP or Start surface.
 */
public class PriceChangeModuleMediator {

    private final ShoppingPersistedTabDataService mShoppingPersistedTabDataService;
    private final FaviconHelper mFaviconHelper;
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mModel;
    private final int mFaviconSize;
    private final Profile mProfile;
    private final ImageFetcher mImageFetcher;

    PriceChangeModuleMediator(
            Context context,
            PropertyModel model,
            Profile profile,
            TabModelSelector tabModelSelector,
            FaviconHelper faviconHelper,
            ImageFetcher imageFetcher) {
        mModel = model;
        mProfile = profile;
        mShoppingPersistedTabDataService = ShoppingPersistedTabDataService.getForProfile(profile);
        mFaviconHelper = faviconHelper;
        mTabModelSelector = tabModelSelector;
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mImageFetcher = imageFetcher;
    }

    /** Show the price change module. */
    public void showModule() {
        if (!mShoppingPersistedTabDataService.isInitialized()) {
            SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
            Set<Tab> tabList = new HashSet<>();
            for (String tabIdString :
                    manager.readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP)) {
                tabList.add(
                        TabModelUtils.getTabById(
                                mTabModelSelector.getModel(false), Integer.valueOf(tabIdString)));
            }

            mShoppingPersistedTabDataService.initialize(tabList);
        }

        mShoppingPersistedTabDataService.getAllShoppingPersistedTabDataWithPriceDrop(
                res -> {
                    if (res.size() == 0) {
                        return;
                    }
                    ShoppingPersistedTabData data = res.get(0).getData();
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING,
                            data.getProductTitle());
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING,
                            data.getPriceDrop().price);
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING,
                            data.getPriceDrop().previousPrice);
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_DOMAIN_STRING,
                            UrlUtilities.getDomainAndRegistry(
                                    res.get(0).getTab().getUrl().getSpec(), false));

                    mFaviconHelper.getLocalFaviconImageForURL(
                            mProfile,
                            res.get(0).getTab().getUrl(),
                            mFaviconSize,
                            (image, iconUrl) -> {
                                mModel.set(
                                        PriceChangeModuleProperties.MODULE_FAVICON_BITMAP, image);
                            });

                    ImageFetcher.Params params =
                            ImageFetcher.Params.create(
                                    data.getProductImageUrl(),
                                    ImageFetcher.PRICE_CHANGE_MODULE_NAME);
                    mImageFetcher.fetchImage(
                            params,
                            image -> {
                                mModel.set(
                                        PriceChangeModuleProperties.MODULE_PRODUCT_IMAGE_BITMAP,
                                        image);
                            });
                });
    }
}
