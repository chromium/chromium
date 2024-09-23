// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.Set;

/**
 * Mediator for the price change module which can be embedded by surfaces like NTP or Start surface.
 */
public class PriceChangeModuleMediator implements TabModelSelectorObserver {

    private final Context mContext;
    private final ShoppingPersistedTabDataService mShoppingPersistedTabDataService;
    private final FaviconHelper mFaviconHelper;
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mModel;
    private final int mFaviconSize;
    private final Profile mProfile;
    private final ImageFetcher mImageFetcher;
    private final ModuleDelegate mModuleDelegate;
    private final @ModuleType int mModuleType;
    private final SharedPreferences mSharedPreferences;
    private final SharedPreferences.OnSharedPreferenceChangeListener mPriceAnnotationsPrefListener;

    PriceChangeModuleMediator(
            Context context,
            PropertyModel model,
            Profile profile,
            TabModelSelector tabModelSelector,
            FaviconHelper faviconHelper,
            ImageFetcher imageFetcher,
            ModuleDelegate moduleDelegate,
            SharedPreferences sharedPreferences) {
        mContext = context;
        mModel = model;
        mProfile = profile;
        mShoppingPersistedTabDataService = ShoppingPersistedTabDataService.getForProfile(profile);
        mFaviconHelper = faviconHelper;
        mTabModelSelector = tabModelSelector;
        mFaviconSize = context.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mImageFetcher = imageFetcher;
        mModuleDelegate = moduleDelegate;
        mModuleType = ModuleType.PRICE_CHANGE;
        mSharedPreferences = sharedPreferences;
        mPriceAnnotationsPrefListener =
                (sharedPrefs, key) -> {
                    if (!PriceTrackingUtilities.TRACK_PRICES_ON_TABS.equals(key)) return;
                    if (!sharedPrefs.getBoolean(
                            PriceTrackingUtilities.TRACK_PRICES_ON_TABS,
                            PriceTrackingFeatures.isPriceTrackingEnabled(profile))) {
                        mModuleDelegate.removeModule(getModuleType());
                    }
                };
        mSharedPreferences.registerOnSharedPreferenceChangeListener(mPriceAnnotationsPrefListener);
    }

    /** Show the price change module. */
    public void showModule() {
        if (!mTabModelSelector.isTabStateInitialized()) {
            mTabModelSelector.addObserver(this);
            return;
        }

        if (!mShoppingPersistedTabDataService.isInitialized()) {
            SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
            Set<Tab> tabList = new HashSet<>();
            for (String tabIdString :
                    manager.readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP)) {
                tabList.add(
                        mTabModelSelector.getModel(false).getTabById(Integer.valueOf(tabIdString)));
            }

            mShoppingPersistedTabDataService.initialize(tabList);
        }

        mShoppingPersistedTabDataService.getAllShoppingPersistedTabDataWithPriceDrop(
                res -> {
                    if (res.size() == 0) {
                        mModuleDelegate.onDataFetchFailed(mModuleType);
                        return;
                    }
                    Tab tab = res.get(0).getTab();
                    // Check if tab is in the current tab model for multi-window case.
                    if (tab == null
                            || mTabModelSelector.getModel(false).getTabById(tab.getId()) == null) {
                        mModuleDelegate.onDataFetchFailed(mModuleType);
                        return;
                    }
                    ShoppingPersistedTabData data = res.get(0).getData();
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_TITLE,
                            mContext.getResources()
                                    .getQuantityString(
                                            org.chromium.chrome.browser.price_change.R.plurals
                                                    .price_change_module_title,
                                            1));
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING,
                            data.getProductTitle());
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING,
                            data.getPriceDrop().price);
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING,
                            data.getPriceDrop().previousPrice);
                    String domain =
                            UrlUtilities.getDomainAndRegistry(
                                    res.get(0).getTab().getUrl().getSpec(), false);
                    mModel.set(PriceChangeModuleProperties.MODULE_DOMAIN_STRING, domain);
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_ACCESSIBILITY_LABEL,
                            mContext.getString(
                                    R.string.price_change_module_accessibility_label,
                                    data.getPriceDrop().previousPrice,
                                    data.getPriceDrop().price,
                                    data.getProductTitle(),
                                    domain));
                    mModel.set(
                            PriceChangeModuleProperties.MODULE_ON_CLICK_LISTENER,
                            v -> mModuleDelegate.onTabClicked(tab.getId(), mModuleType));

                    // No need to wait for image and favicon ready to notify that the module is
                    // ready.
                    mModuleDelegate.onDataReady(mModuleType, mModel);

                    mFaviconHelper.getLocalFaviconImageForURL(
                            mProfile,
                            tab.getUrl(),
                            mFaviconSize,
                            (image, iconUrl) -> {
                                if (image != null) {
                                    mModel.set(
                                            PriceChangeModuleProperties.MODULE_FAVICON_BITMAP,
                                            image);
                                    return;
                                }
                                Drawable drawable =
                                        AppCompatResources.getDrawable(
                                                mContext, R.drawable.ic_globe_24dp);
                                Bitmap bitmap =
                                        Bitmap.createBitmap(
                                                mFaviconSize,
                                                mFaviconSize,
                                                Bitmap.Config.ARGB_8888);
                                Canvas canvas = new Canvas(bitmap);
                                drawable.setBounds(0, 0, mFaviconSize, mFaviconSize);
                                drawable.draw(canvas);
                                mModel.set(
                                        PriceChangeModuleProperties.MODULE_FAVICON_BITMAP, bitmap);
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

    void destroy() {
        mSharedPreferences.unregisterOnSharedPreferenceChangeListener(
                mPriceAnnotationsPrefListener);
        mTabModelSelector.removeObserver(this);
    }

    int getModuleType() {
        return mModuleType;
    }

    @Override
    public void onTabStateInitialized() {
        mTabModelSelector.removeObserver(this);
        showModule();
    }
}
