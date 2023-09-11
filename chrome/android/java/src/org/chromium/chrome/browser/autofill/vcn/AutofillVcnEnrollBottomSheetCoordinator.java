// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator controller for the virtual card number (VCN) enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetCoordinator {
    /** Callbacks from the VCN enrollment bottom sheet. */
    interface Delegate {
        /** Called when the user accepts the VCN enrollment bottom sheet. */
        void onAccept();

        /** Called when the user cancels the VCN enrollment bottom sheet. */
        void onCancel();

        /** Called when the user dismisses the VCN enrollment bottom sheet. */
        void onDismiss();
    }

    private final AutofillVcnEnrollBottomSheetMediator mMediator;

    /**
     * Constructs a coordinator controller for the virtual card enrollment bottom sheet.
     *
     * @param context The activity context.
     * @param modelBuilder The bottom sheet contents.
     * @param layoutStateProvider Exposes a way to listen to layout state changes.
     * @param tabModelSelectorSupplier Supplies the tab model selector when it's ready.
     * @param delegate The callbacks for user actions.
     */
    AutofillVcnEnrollBottomSheetCoordinator(Context context, PropertyModel.Builder modelBuilder,
            LayoutStateProvider layoutStateProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier, Delegate delegate) {
        AutofillVcnEnrollBottomSheetView view = new AutofillVcnEnrollBottomSheetView(context);
        view.mAcceptButton.setOnClickListener((View button) -> delegate.onAccept());
        view.mCancelButton.setOnClickListener((View button) -> delegate.onCancel());

        PropertyModelChangeProcessor.create(
                modelBuilder.build(), view, AutofillVcnEnrollBottomSheetViewBinder::bind);

        mMediator = new AutofillVcnEnrollBottomSheetMediator(
                new AutofillVcnEnrollBottomSheetContent(
                        view.mContentView, view.mScrollView, delegate::onDismiss),
                new AutofillVcnEnrollBottomSheetLifecycle(
                        layoutStateProvider, tabModelSelectorSupplier));
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    boolean requestShowContent(WindowAndroid window) {
        return mMediator.requestShowContent(window);
    }

    /** Hides the virtual card enrollment bottom sheet, if present. */
    void hide() {
        mMediator.hide();
    }
}
