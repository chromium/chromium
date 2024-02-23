// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link ModuleProviderBuilder} that builds the price change module. */
public class PriceChangeModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final TabModelSelector mTabModelSelector;

    /** Pass in the dependencies needed to build {@link PriceChangeModuleCoordinator}. */
    public PriceChangeModuleBuilder(
            @NonNull Context context,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull TabModelSelector tabModelSelector) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mTabModelSelector = tabModelSelector;
    }

    /** Build {@link ModuleProvider} for the price change module. */
    @Override
    public boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        Profile profile = getRegularProfile();
        if (!PriceTrackingUtilities.isTrackPricesOnTabsEnabled(profile)) {
            return false;
        }
        PriceChangeModuleCoordinator coordinator =
                new PriceChangeModuleCoordinator(
                        mContext, profile, mTabModelSelector, moduleDelegate);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the price change module. */
    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.price_change_module_layout, parentView, false);
    }

    /** Bind the property model for the price change module. */
    @Override
    public void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey) {
        PriceChangeModuleViewBinder.bind(model, view, propertyKey);
    }

    // ModuleEligibilityChecker implementation:

    @Override
    public boolean isEligible() {
        // This function may be called by MainSettings when a profile hasn't been initialized yet.
        // See b/324138242.
        if (!mProfileSupplier.hasValue()) return false;

        return PriceTrackingUtilities.isTrackPricesOnTabsEnabled(getRegularProfile());
    }

    /** Gets the regular profile if exists. */
    @VisibleForTesting
    Profile getRegularProfile() {
        assert mProfileSupplier.hasValue();

        Profile profile = mProfileSupplier.get();
        // It is possible that an incognito profile is provided by the supplier. See b/326619334.
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }
}
