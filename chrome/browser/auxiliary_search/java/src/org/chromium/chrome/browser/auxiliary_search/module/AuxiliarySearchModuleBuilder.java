// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Builder to build the auxiliary search opt in module. */
public class AuxiliarySearchModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;

    public AuxiliarySearchModuleBuilder(@NonNull Context context) {
        mContext = context;
    }

    // ModuleProviderBuilder implementations.

    @Override
    public boolean build(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull Callback<ModuleProvider> onModuleBuiltCallback) {
        if (!ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled()) {
            return false;
        }

        AuxiliarySearchModuleCoordinator coordinator =
                new AuxiliarySearchModuleCoordinator(moduleDelegate);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
                        .inflate(R.layout.auxiliary_search_module_layout, parentView, false);
    }

    @Override
    public void bind(
            @NonNull PropertyModel model,
            @NonNull ViewGroup view,
            @NonNull PropertyKey propertyKey) {
        AuxiliarySearchModuleViewBinder.bind(model, view, propertyKey);
    }

    @Override
    public boolean isEligible() {
        return ChromeFeatureList.sAndroidAppIntegrationWithFavicon.isEnabled();
    }
}
