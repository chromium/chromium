// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the educational tip module. */
public class EducationalTipModuleCoordinator implements ModuleProvider {
    private final EducationalTipModuleMediator mMediator;

    public EducationalTipModuleCoordinator(
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull EducationTipModuleActionDelegate actionDelegate) {
        PropertyModel model = new PropertyModel(EducationalTipModuleProperties.ALL_KEYS);
        mMediator = new EducationalTipModuleMediator(model, moduleDelegate, actionDelegate);
    }

    // ModuleProvider implementation.

    /** Show educational tip module. */
    @Override
    public void showModule() {
        mMediator.showModule();
    }

    @Override
    public int getModuleType() {
        return mMediator.getModuleType();
    }

    @Override
    public void hideModule() {
        mMediator.destroy();
    }

    @Override
    public String getModuleContextMenuHideText(@NonNull Context context) {
        return context.getString(R.string.educational_tip_module_context_menu_hide);
    }

    @Override
    public void onContextMenuCreated() {}
}
