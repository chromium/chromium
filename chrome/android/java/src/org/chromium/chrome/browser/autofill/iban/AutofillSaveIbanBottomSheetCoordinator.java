// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.content.Context;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveIbanUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the autofill IBAN save UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a IBAN.
 */
public class AutofillSaveIbanBottomSheetCoordinator {
    private final AutofillSaveIbanBottomSheetMediator mMediator;
    private final AutofillSaveIbanBottomSheetView mView;
    private PropertyModel mModel;

    /**
     * Creates the coordinator.
     *
     * @param bridge The bridge to signal UI flow events (OnUiCanceled, OnUiAccepted, etc.) to.
     * @param uiInfo An object providing UI resources for the bottom sheet model.
     * @param context The context for this component.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     */
    public AutofillSaveIbanBottomSheetCoordinator(
            AutofillSaveIbanBottomSheetBridge bridge,
            AutofillSaveIbanUiInfo uiInfo,
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel) {
        mView = new AutofillSaveIbanBottomSheetView(context);

        mModel =
                new PropertyModel.Builder(AutofillSaveIbanBottomSheetProperties.ALL_KEYS)
                        .with(AutofillSaveIbanBottomSheetProperties.TITLE, uiInfo.getTitleText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.IBAN_LABEL,
                                uiInfo.getIbanLabel())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL,
                                uiInfo.getAcceptText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                uiInfo.getCancelText())
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, AutofillSaveIbanBottomSheetViewBinder::bind);

        mMediator =
                new AutofillSaveIbanBottomSheetMediator(
                        bridge,
                        new AutofillSaveIbanBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel);
    }

    /** Request to show the bottom sheet. */
    void requestShowContent() {
        mMediator.requestShowContent();
    }

    /** Destroys this component, hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.destroy();
    }

    /** Retrieves the PropertyModel associated with the view for testing purposes. */
    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }
}
