// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.TAB_COUNT;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the {@link TabSwitcherDrawable} for the {@link TabSwitcherPane}. */
public class TabSwitcherPaneDrawableMediator {
    private final PropertyModel mModel;
    private final Callback<Integer> mTabCountSupplierObserver;

    private ObservableSupplier<Integer> mTabCountSupplier;

    private CallbackController mCallbackController = new CallbackController();

    public TabSwitcherPaneDrawableMediator(
            @NonNull TabModelSelector tabModelSelector, @NonNull PropertyModel model) {
        mModel = model;

        mTabCountSupplierObserver =
                tabCount -> {
                    mModel.set(TAB_COUNT, tabCount);
                };

        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(this::onTabStateInitializedInternal));
    }

    /** Destroys the mediator, removing observers if present. */
    public void destroy() {
        mCallbackController.destroy();
        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountSupplierObserver);
            mTabCountSupplier = null;
        }
    }

    private void onTabStateInitializedInternal(@NonNull TabModelSelector tabModelSelector) {
        mTabCountSupplier = tabModelSelector.getModel(false).getTabCountSupplier();
        mTabCountSupplier.addObserver(mTabCountSupplierObserver);
    }
}
