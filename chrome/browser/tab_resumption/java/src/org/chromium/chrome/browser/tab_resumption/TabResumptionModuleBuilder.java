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
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class TabResumptionModuleBuilder implements ModuleProviderBuilder {
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
        Profile profile = mProfileSupplier.get();
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

    @Override
    public boolean isEligible() {
        Profile profile = mProfileSupplier.get();
        return TabResumptionModuleUtils.shouldShowTabResumptionModule(profile);
    }
}
