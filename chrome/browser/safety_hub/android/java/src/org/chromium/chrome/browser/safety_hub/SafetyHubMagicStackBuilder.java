// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** {@link ModuleProviderBuilder} that builds the Safety Hub Magic Stack module. */
@NullMarked
public class SafetyHubMagicStackBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    public SafetyHubMagicStackBuilder(
            Context context,
            ObservableSupplier<Profile> profileSupplier,
            TabModelSelector tabModelSelector,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mContext = context;
        mProfileSupplier = profileSupplier;
        mTabModelSelector = tabModelSelector;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;

        recordMetricForMagicStackSettingState();
    }

    /**
     * Records the metric related to the settings state of the safety hub magic stack module. This
     * should only be recorded on start up.
     */
    private void recordMetricForMagicStackSettingState() {
        boolean magicStackModuleEnabled =
                HomeModulesConfigManager.getInstance()
                        .getPrefModuleTypeEnabled(ModuleType.SAFETY_HUB);
        RecordHistogram.recordBooleanHistogram(
                "Settings.SafetyHub.MagicStack.StateOnStartup", magicStackModuleEnabled);
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
                        mModalDialogManagerSupplier);
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
        // The Safety Hub is not fully supported on Automotive.
        if (DeviceInfo.isAutomotive()) return false;

        Profile profile = mProfileSupplier.get();
        if (profile == null) return false;

        return true;
    }

    private Profile getRegularProfile() {
        Profile profile = mProfileSupplier.get();
        assumeNonNull(profile);
        // It is possible that an incognito profile is provided by the supplier. See b/326619334.
        return profile.isOffTheRecord() ? profile.getOriginalProfile() : profile;
    }
}
