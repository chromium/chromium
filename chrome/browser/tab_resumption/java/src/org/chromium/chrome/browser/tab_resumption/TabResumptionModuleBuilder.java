// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class TabResumptionModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;

    public TabResumptionModuleBuilder(
            @NonNull Context context, @NonNull ObservableSupplier<Profile> profileSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
    }

    /** Build {@link ModuleProvider} for the tab resumption module. */
    @Override
    public boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        Profile profile = getRegularProfile();
        if (!TabResumptionModuleUtils.shouldShowTabResumptionModule(profile)) {
            return false;
        }

        TabResumptionDataProvider dataProvider =
                ForeignSessionTabResumptionDataProvider.createFromProfile(profile);
        UrlImageProvider urlImageProvider = new UrlImageProvider(profile, mContext);
        TabResumptionModuleCoordinator coordinator =
                new TabResumptionModuleCoordinator(
                        mContext, moduleDelegate, dataProvider, urlImageProvider);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the tab resumption module. */
    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.tab_resumption_module_layout, parentView, false);
    }

    /** Bind the property model for the tab resumption module. */
    @Override
    public void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey) {
        TabResumptionModuleViewBinder.bind(model, view, propertyKey);
    }

    // ModuleEligibilityChecker implementation:

    @Override
    public boolean isEligible() {
        // This function may be called by MainSettings when a profile hasn't been initialized yet.
        // See b/324138242.
        if (!mProfileSupplier.hasValue()) return false;

        return TabResumptionModuleUtils.shouldShowTabResumptionModule(getRegularProfile());
    }

    /** Gets the regular profile if exists. */
    private Profile getRegularProfile() {
        assert mProfileSupplier.hasValue();

        Profile profile = mProfileSupplier.get();
        // It is possible that an incognito profile is provided by the supplier. See b/326619334.
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }
}
