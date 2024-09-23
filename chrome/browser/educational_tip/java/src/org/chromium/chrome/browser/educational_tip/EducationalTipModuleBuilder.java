// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class EducationalTipModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final EducationTipModuleActionDelegate mActionDelegate;

    /** Pass in the dependencies needed to build {@link EducationalTipModuleCoordinator}. */
    public EducationalTipModuleBuilder(@NonNull EducationTipModuleActionDelegate actionDelegate) {
        mActionDelegate = actionDelegate;
    }

    /** Build {@link ModuleProvider} for the educational tip module. */
    @Override
    public boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!ChromeFeatureList.sEducationalTipModule.isEnabled()) {
            return false;
        }

        EducationalTipModuleCoordinator coordinator =
                new EducationalTipModuleCoordinator(moduleDelegate, mActionDelegate);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the educational tip module. */
    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mActionDelegate.getContext())
                        .inflate(R.layout.educational_tip_module_layout, parentView, false);
    }

    /** Bind the property model for the educational tip module. */
    @Override
    public void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey) {
        EducationalTipModuleViewBinder.bind(model, view, propertyKey);
    }

    // ModuleEligibilityChecker implementation:

    @Override
    public boolean isEligible() {
        return ChromeFeatureList.sEducationalTipModule.isEnabled();
    }
}
