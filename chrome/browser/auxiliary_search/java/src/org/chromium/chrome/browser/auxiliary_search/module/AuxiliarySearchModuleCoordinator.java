// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search.module;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.auxiliary_search.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the auxiliary search opt in module. */
@NullMarked
public class AuxiliarySearchModuleCoordinator implements ModuleProvider {
    private final AuxiliarySearchModuleMediator mMediator;

    /**
     * @param moduleDelegate The instance of {@link ModuleDelegate}, which is the magic stack.
     * @param openSettingsRunnable The runnable to open the Tabs settings.
     */
    public AuxiliarySearchModuleCoordinator(
            ModuleDelegate moduleDelegate, Runnable openSettingsRunnable) {
        PropertyModel model = new PropertyModel(AuxiliarySearchModuleProperties.ALL_KEYS);
        mMediator = new AuxiliarySearchModuleMediator(model, moduleDelegate, openSettingsRunnable);
    }

    // ModuleProvider implementations.

    @Override
    public void showModule() {
        mMediator.showModule();
    }

    @Override
    public void hideModule() {
        mMediator.hideModule();
    }

    @Override
    public int getModuleType() {
        return mMediator.getModuleType();
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return context.getString(R.string.auxiliary_search_module_context_menu_hide);
    }
}
