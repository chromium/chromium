// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.app.Activity;
import android.content.res.Resources;

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
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.feature_engagement.FeatureConstants;

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
                AppCompatResources.getDrawable(mActivity, R.drawable.price_tracking_disabled),
                view
                -> onPriceTrackingClicked(tabSupplier.get()),
                R.string.enable_price_tracking_menu_item, /*supportsTinting=*/true,
                /*iphCommandBuilder*/ null,
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
        maybeSetActionChipResourceId();
        return mButtonData;
    }

    private void maybeSetActionChipResourceId() {
        if (FeatureList.isInitialized() && AdaptiveToolbarFeatures.shouldShowActionChip()) {
            // OptionalButtonCoordinator may choose to not show this action chip. It uses feature
            // engagement to rate limit this animation.
            mButtonData.updateActionChipResourceId(R.string.enable_price_tracking_menu_item);
        } else {
            mButtonData.updateActionChipResourceId(Resources.ID_NULL);
        }
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

        IPHCommandBuilder iphCommandBuilder = new IPHCommandBuilder(tab.getContext().getResources(),
                FeatureConstants.CONTEXTUAL_PAGE_ACTIONS_PRICE_TRACKING,
                /* stringId = */ R.string.iph_price_tracking_menu_item,
                /* accessibilityStringId = */ R.string.iph_price_tracking_menu_item);
        mButtonData.updateIPHCommandBuilder(iphCommandBuilder);
    }
}
