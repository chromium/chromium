// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the auxiliary search opt in module. */
public class AuxiliarySearchModuleMediator {
    private final PropertyModel mModel;
    private final ModuleDelegate mModuleDelegate;

    /**
     * @param model The instance of {@link PropertyModel} for the view.
     * @param moduleDelegate The instance of {@link ModuleDelegate}, which is the magic stack.
     */
    public AuxiliarySearchModuleMediator(
            @NonNull PropertyModel model, @NonNull ModuleDelegate moduleDelegate) {
        mModel = model;
        mModuleDelegate = moduleDelegate;
    }

    void showModule() {
        mModuleDelegate.onDataReady(ModuleType.AUXILIARY_SEARCH, mModel);
    }

    void hideModule() {}

    int getModuleType() {
        return ModuleType.AUXILIARY_SEARCH;
    }
}
