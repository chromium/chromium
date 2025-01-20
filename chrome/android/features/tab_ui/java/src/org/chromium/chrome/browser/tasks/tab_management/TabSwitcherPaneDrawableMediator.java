// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.SHOW_NOTIFICATION_DOT;
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
    private final CallbackController mCallbackController = new CallbackController();
    private final Callback<Boolean> mNotificationDotObserver = this::updateNotificationDot;
    private final Callback<Integer> mTabCountSupplierObserver = this::updateTabCount;
    private final ObservableSupplier<Boolean> mNotificationDotSupplier;
    private final PropertyModel mModel;

    private ObservableSupplier<Integer> mTabCountSupplier;

    public TabSwitcherPaneDrawableMediator(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ObservableSupplier<Boolean> notificationDotSupplier,
            @NonNull PropertyModel model) {
        mNotificationDotSupplier = notificationDotSupplier;
        mModel = model;

        notificationDotSupplier.addObserver(mNotificationDotObserver);
        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(this::onTabStateInitializedInternal));
    }

    /** Destroys the mediator, removing observers if present. */
    public void destroy() {
        mCallbackController.destroy();
        mNotificationDotSupplier.removeObserver(mNotificationDotObserver);
        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountSupplierObserver);
            mTabCountSupplier = null;
        }
    }

    private void onTabStateInitializedInternal(@NonNull TabModelSelector tabModelSelector) {
        mTabCountSupplier = tabModelSelector.getModel(false).getTabCountSupplier();
        mTabCountSupplier.addObserver(mTabCountSupplierObserver);
    }

    private void updateNotificationDot(boolean showDot) {
        mModel.set(SHOW_NOTIFICATION_DOT, showDot);
    }

    private void updateTabCount(int tabCount) {
        mModel.set(TAB_COUNT, tabCount);
    }
}
