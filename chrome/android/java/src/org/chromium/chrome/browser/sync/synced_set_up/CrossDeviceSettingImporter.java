// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.synced_set_up;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

@NullMarked
public class CrossDeviceSettingImporter implements TopResumedActivityChangedObserver {

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final NullableObservableSupplier<Tab> mActivityTabSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Callback<@Nullable Tab> mTabChangeCallback =
            new Callback<@Nullable Tab>() {
                @Override
                public void onResult(@Nullable Tab tab) {
                    onTabChangeOrGainFocus(tab);
                }
            };

    public CrossDeviceSettingImporter(
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            NullableObservableSupplier<Tab> activityTabSupplier,
            Supplier<ModalDialogManager> modalDialogManager,
            Supplier<SnackbarManager> snackbarManagerSupplier) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityTabSupplier = activityTabSupplier;
        mModalDialogManagerSupplier = modalDialogManager;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mActivityLifecycleDispatcher.register(this);
        mActivityTabSupplier.addObserver(mTabChangeCallback);
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        if (!isTopResumedActivity) return;
        onTabChangeOrGainFocus(mActivityTabSupplier.get());
    }

    private void onTabChangeOrGainFocus(@Nullable Tab currentTab) {
        if (currentTab == null) return;
        // TODO(crbug.com/475543024): Implement.
    }

    /**
     * Shows {@param snackbar} now if there no dialogs, or waits until the last dialog is dismissed
     * and then shows it.
     */
    @VisibleForTesting
    public void showSnackbarAfterDialogs(Snackbar snackbar) {
        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        if (modalDialogManager == null) return;

        SnackbarManager snackbarManager = mSnackbarManagerSupplier.get();
        if (snackbarManager == null) return;

        if (modalDialogManager.isShowing()) {
            modalDialogManager.addObserver(
                    new ModalDialogManager.ModalDialogManagerObserver() {
                        @Override
                        public void onLastDialogDismissed() {
                            snackbarManager.showSnackbar(snackbar);
                        }
                    });
        } else {
            snackbarManager.showSnackbar(snackbar);
        }
    }

    public void destroy() {
        mActivityLifecycleDispatcher.unregister(this);
        mActivityTabSupplier.removeObserver(mTabChangeCallback);
    }
}
