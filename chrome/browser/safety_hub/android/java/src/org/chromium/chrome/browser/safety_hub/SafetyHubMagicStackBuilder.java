// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link ModuleProviderBuilder} that builds the Safety Hub Magic Stack module. */
public class SafetyHubMagicStackBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    public SafetyHubMagicStackBuilder(
            @NonNull Context context,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mTabModelSelector = tabModelSelector;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @Override
    public boolean build(
            ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
        Profile profile = getRegularProfile();
        SafetyHubMagicStackCoordinator coordinator =
                new SafetyHubMagicStackCoordinator(
                        mContext,
                        profile,
                        mTabModelSelector,
                        moduleDelegate,
                        mModalDialogManagerSupplier,
                        this::showProactiveSurvey);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    @Override
    public ViewGroup createView(ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.safety_hub_magic_stack_view, parentView, false);
    }

    @Override
    public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        SafetyHubMagicStackViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public boolean isEligible() {
        if (!mProfileSupplier.hasValue()) return false;

        if (!ChromeFeatureList.sSafetyHub.isEnabled()) {
            SafetyHubHatsBridge.getForProfile(getRegularProfile())
                    .triggerControlHatsSurvey(mTabModelSelector);
        }
        return ChromeFeatureList.sSafetyHub.isEnabled();
    }

    private void showProactiveSurvey(String moduleType) {
        SafetyHubHatsBridge.getForProfile(getRegularProfile())
                .triggerProactiveHatsSurvey(mTabModelSelector, moduleType);
    }

    private Profile getRegularProfile() {
        assert mProfileSupplier.hasValue();

        Profile profile = mProfileSupplier.get();
        // It is possible that an incognito profile is provided by the supplier. See b/326619334.
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }
}
