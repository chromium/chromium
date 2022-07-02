// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.FeatureList;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataImpl;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures.AdaptiveToolbarButtonVariant;

/**
 * Responsible for providing UI resources for showing price tracking action on optional toolbar
 * button.
 */
public class PriceTrackingButtonController implements ButtonDataProvider {
    private final Activity mActivity;
    private final Supplier<TabBookmarker> mTabBookmarkerSupplier;
    private final ObserverList<ButtonDataObserver> mObservers = new ObserverList<>();
    private final ButtonDataImpl mButtonData;

    /** Constructor. */
    public PriceTrackingButtonController(Activity activity, ObservableSupplier<Tab> tabSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier) {
        mActivity = activity;
        mTabBookmarkerSupplier = tabBookmarkerSupplier;

        // TODO(shaktisahu): Provide accurate icon and string.
        mButtonData = new ButtonDataImpl(/*canShow=*/true,
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_toolbar_share_offset_24dp),
                view
                -> onPriceTrackingClicked(tabSupplier.get()),
                R.string.share, /*supportsTinting=*/true, /*iphCommandBuilder*/ null,
                /*isEnabled=*/true, AdaptiveToolbarButtonVariant.PRICE_TRACKING);
    }

    @Override
    public void addObserver(ButtonDataObserver obs) {
        mObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(ButtonDataObserver obs) {
        mObservers.removeObserver(obs);
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        maybeSetIphCommandBuilder(tab);
        return mButtonData;
    }

    @Override
    public void destroy() {
        mObservers.clear();
    }

    private void onPriceTrackingClicked(Tab tab) {
        mTabBookmarkerSupplier.get().startOrModifyPriceTracking(tab);
    }

    private void maybeSetIphCommandBuilder(Tab tab) {
        if (mButtonData.getButtonSpec().getIPHCommandBuilder() != null || tab == null
                || !FeatureList.isInitialized() || AdaptiveToolbarFeatures.shouldShowActionChip()) {
            return;
        }

        // TODO(shaktisahu): Implement IPH and update button data.
    }
}
