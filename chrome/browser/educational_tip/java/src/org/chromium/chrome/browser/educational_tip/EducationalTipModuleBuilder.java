// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class EducationalTipModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final @ModuleType int mModuleType;
    private @Nullable Profile mProfile;

    /** Pass in the dependencies needed to build {@link EducationalTipModuleCoordinator}. */
    public EducationalTipModuleBuilder(
            @ModuleType int moduleTypeToBuild, EducationTipModuleActionDelegate actionDelegate) {
        mModuleType = moduleTypeToBuild;
        mActionDelegate = actionDelegate;
    }

    /** Build {@link ModuleProvider} for the educational tip module. */
    @Override
    public boolean build(
            ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!ChromeFeatureList.sEducationalTipModule.isEnabled()
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER)) {
            return false;
        }

        if (mModuleType == ModuleType.DEFAULT_BROWSER_PROMO
                && !ChromeFeatureList.sEducationalTipDefaultBrowserPromoCard.isEnabled()) {
            return false;
        }

        EducationalTipModuleCoordinator coordinator =
                new EducationalTipModuleCoordinator(
                        mModuleType,
                        moduleDelegate,
                        mActionDelegate,
                        getRegularProfile(mActionDelegate.getProfileSupplier()));
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the educational tip module. */
    @Override
    public ViewGroup createView(ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mActionDelegate.getContext())
                        .inflate(R.layout.educational_tip_module_layout, parentView, false);
    }

    /** Bind the property model for the educational tip module. */
    @Override
    public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        EducationalTipModuleViewBinder.bind(model, view, propertyKey);
    }

    // ModuleEligibilityChecker implementation:

    @Override
    public boolean isEligible() {
        return ChromeFeatureList.sEducationalTipModule.isEnabled();
    }

    @Override
    public InputContext createInputContext() {
        Profile profile = getRegularProfile(mActionDelegate.getProfileSupplier());
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        return EducationalTipCardProviderSignalHandler.createInputContext(
                mModuleType, mActionDelegate, profile, tracker);
    }

    /** Gets the regular profile if exists. */
    private Profile getRegularProfile(ObservableSupplier<Profile> profileSupplier) {
        if (mProfile != null) {
            return mProfile;
        }

        assert profileSupplier.hasValue();
        mProfile = profileSupplier.get().getOriginalProfile();
        return mProfile;
    }
}
