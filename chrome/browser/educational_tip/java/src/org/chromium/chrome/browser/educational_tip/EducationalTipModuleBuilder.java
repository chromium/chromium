// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleConfigChecker;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder;
import org.chromium.chrome.browser.tab_ui.TabGridIphDialogCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class EducationalTipModuleBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Runnable mShowTabSwitcher;
    private final Supplier<ViewGroup> mParentViewSupplier;

    private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;

    /** Pass in the dependencies needed to build {@link EducationalTipModuleCoordinator}. */
    public EducationalTipModuleBuilder(
            @NonNull Context context,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Runnable showTabSwitcherRunnable,
            @NonNull Supplier<ViewGroup> parentViewSupplier) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mShowTabSwitcher = showTabSwitcherRunnable;
        mParentViewSupplier = parentViewSupplier;
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
                new EducationalTipModuleCoordinator(
                        mContext,
                        moduleDelegate,
                        mBottomSheetController,
                        mModalDialogManagerSupplier,
                        mShowTabSwitcher,
                        mParentViewSupplier);
        onModuleBuiltCallback.onResult(coordinator);
        return true;
    }

    /** Create view for the educational tip module. */
    @Override
    public ViewGroup createView(@NonNull ViewGroup parentView) {
        return (ViewGroup)
                LayoutInflater.from(mContext)
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
