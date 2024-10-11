// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.content.Context;
import android.widget.EditText;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
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
    /** Native callbacks for the IBAN bottom sheet. */
    public interface NativeDelegate {
        /**
         * Called when the save button has been clicked.
         *
         * @param userProvidedNickname The nickname provided by the user when the "Save" button is
         *     clicked.
         */
        void onUiAccepted(String userProvidedNickname);

        /** Called when the user clicks on the "No thanks" button. */
        void onUiCanceled();

        /** Called when the user ignores the save IBAN bottom sheet. */
        void onUiIgnored();
    }

    private final Context mContext;
    private final AutofillSaveIbanBottomSheetMediator mMediator;
    private final AutofillSaveIbanBottomSheetView mView;
    private PropertyModel mModel;
    protected EditText mNickname;

    /**
     * Creates the coordinator.
     *
     * @param delegate The delegate to signal UI flow events (OnUiCanceled, OnUiAccepted, etc.) to.
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
            NativeDelegate delegate,
            AutofillSaveIbanUiInfo uiInfo,
            Context context,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel) {
        mContext = context;
        mView = new AutofillSaveIbanBottomSheetView(context);
        mModel =
                new PropertyModel.Builder(AutofillSaveIbanBottomSheetProperties.ALL_KEYS)
                        .with(AutofillSaveIbanBottomSheetProperties.LOGO_ICON, uiInfo.getLogoIcon())
                        .with(AutofillSaveIbanBottomSheetProperties.TITLE, uiInfo.getTitleText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.DESCRIPTION,
                                uiInfo.getDescriptionText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.IBAN_VALUE,
                                uiInfo.getIbanValue())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL,
                                uiInfo.getAcceptText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                uiInfo.getCancelText())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.ON_ACCEPT_BUTTON_CLICK_ACTION,
                                v ->
                                        this.onAcceptButtonClick(
                                                mView.mNickname.getText().toString().trim()))
                        .with(
                                AutofillSaveIbanBottomSheetProperties.ON_CANCEL_BUTTON_CLICK_ACTION,
                                v -> this.onCancelButtonClick())
                        .with(
                                AutofillSaveIbanBottomSheetProperties.LEGAL_MESSAGE,
                                new AutofillSaveIbanBottomSheetProperties.LegalMessage(
                                        uiInfo.getLegalMessageLines(), this::openLegalMessageLink))
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, AutofillSaveIbanBottomSheetViewBinder::bind);

        mMediator =
                new AutofillSaveIbanBottomSheetMediator(
                        delegate,
                        new AutofillSaveIbanBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        bottomSheetController,
                        layoutStateProvider,
                        tabModel,
                        uiInfo.isServerSave());
    }

    void onAcceptButtonClick(String userProvidedNickname) {
        mMediator.onAccepted(userProvidedNickname);
    }

    void onCancelButtonClick() {
        mMediator.onCanceled();
    }

    /** Request to show the bottom sheet. */
    void requestShowContent() {
        mMediator.requestShowContent();
    }

    /** Destroys this component, hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.hide(BottomSheetController.StateChangeReason.NONE);
    }

    void openLegalMessageLink(String url) {
        CustomTabActivity.showInfoPage(mContext, url);
    }

    /** Retrieves the PropertyModel associated with the view for testing purposes. */
    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    AutofillSaveIbanBottomSheetView getAutofillSaveIbanBottomSheetViewForTesting() {
        return mView;
    }
}
