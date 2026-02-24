// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Builder for a generic two-cell educational tip module. This module displays two tip items stacked
 * vertically. Used, for example, by the Setup List.
 */
@NullMarked
public class EducationalTipModuleTwoCellBuilder implements ModuleProviderBuilder {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final @ModuleType int mModuleType;

    /**
     * @param moduleType The type of the module to build.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     */
    public EducationalTipModuleTwoCellBuilder(
            @ModuleType int moduleType, EducationTipModuleActionDelegate actionDelegate) {
        mModuleType = moduleType;
        mActionDelegate = actionDelegate;
    }

    // ModuleProviderBuilder implementation.
    @Override
    public boolean build(
            ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
        onModuleBuiltCallback.onResult(
                new EducationalTipModuleTwoCellCoordinator(
                        mModuleType, moduleDelegate, mActionDelegate));
        return true;
    }

    @Override
    public ViewGroup createView(ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(parentView.getContext())
                        .inflate(
                                R.layout.educational_tip_module_two_cell_layout, parentView, false);
    }

    @Override
    public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        EducationalTipModuleTwoCellViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public @Nullable Integer getManualRank() {
        return SetupListModuleUtils.getManualRank(mModuleType);
    }

    @Override
    public boolean isEligible() {
        return SetupListModuleUtils.isModuleEligible(mModuleType);
    }
}
