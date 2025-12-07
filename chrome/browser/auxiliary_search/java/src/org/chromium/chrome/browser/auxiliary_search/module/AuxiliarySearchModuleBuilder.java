// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.components.segmentation_platform.InputContext;
import org.chromium.components.segmentation_platform.ProcessedValue;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Builder to build the auxiliary search opt in module. */
@NullMarked
public class AuxiliarySearchModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private static final String CARD_AVAILABILITY_INPUT_NAME = "auxiliary_search_available";

    private final Context mContext;
    private final Runnable mOpenSettingsRunnable;
    private static boolean sShownInThisSession;

    public AuxiliarySearchModuleBuilder(Context context, Runnable openSettingsRunnable) {
        mContext = context;
        mOpenSettingsRunnable = openSettingsRunnable;
    }

    // ModuleProviderBuilder implementations.

    @Override
    public boolean build(
            ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!AuxiliarySearchUtils.canShowCard(sShownInThisSession)) {
            return false;
        }

        AuxiliarySearchModuleCoordinator coordinator =
                new AuxiliarySearchModuleCoordinator(moduleDelegate, mOpenSettingsRunnable);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    @Override
    public ViewGroup createView(ViewGroup parentView) {
        sShownInThisSession = true;

        ViewGroup viewGroup =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.auxiliary_search_module_layout, parentView, false);
        AuxiliarySearchUtils.incrementModuleImpressions();

        return viewGroup;
    }

    @Override
    public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        AuxiliarySearchModuleViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public boolean isEligible() {
        return ChromeFeatureList.sAndroidAppIntegrationModule.isEnabled()
                && AuxiliarySearchControllerFactory.getInstance().isEnabledAndDeviceCompatible();
    }

    @Override
    public @Nullable InputContext createInputContext() {
        InputContext inputContext = new InputContext();
        float available = 0;
        if (isEligible() && AuxiliarySearchUtils.canShowCard(sShownInThisSession)) {
            available = 1;
        }
        inputContext.addEntry(CARD_AVAILABILITY_INPUT_NAME, ProcessedValue.fromFloat(available));
        return inputContext;
    }

    static void resetShownInThisSessionForTesting() {
        sShownInThisSession = false;
    }
}
