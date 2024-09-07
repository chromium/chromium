// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator for the educational tip module. */
public class EducationalTipModuleCoordinator implements ModuleProvider {
    private final EducationalTipModuleMediator mMediator;

    public EducationalTipModuleCoordinator(
            @NonNull Context context,
            @NonNull ModuleDelegate moduleDelegate,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Runnable showTabSwitcherRunnable,
            @NonNull Supplier<ViewGroup> parentViewSupplier) {
        PropertyModel model = new PropertyModel(EducationalTipModuleProperties.ALL_KEYS);
        mMediator =
                new EducationalTipModuleMediator(
                        context,
                        model,
                        moduleDelegate,
                        bottomSheetController,
                        modalDialogManagerSupplier,
                        showTabSwitcherRunnable,
                        parentViewSupplier);
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
