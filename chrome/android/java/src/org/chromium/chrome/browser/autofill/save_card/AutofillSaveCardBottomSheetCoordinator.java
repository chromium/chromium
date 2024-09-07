// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import android.content.Context;
import android.net.Uri;
import android.view.View;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator of the autofill save card UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a payment card (either
 * locally or uploaded).
 */
public class AutofillSaveCardBottomSheetCoordinator {
    /** Native callbacks from the save card bottom sheet. */
    public interface NativeDelegate {
        /** Called when the save card bottom sheet is shown to the user. */
        void onUiShown();

        /** Called when the user accepts the save card bottom sheet. */
        void onUiAccepted();

        /** Called when the user cancels the save card bottom sheet. */
        void onUiCanceled();

        /** Called when the user ignores the save card bottom sheet. */
        void onUiIgnored();
    }

    private final Context mContext;
    private final AutofillSaveCardBottomSheetView mView;
    private final AutofillSaveCardBottomSheetMediator mMediator;
    private PropertyModel mModel;

    /**
     * Creates the coordinator.
     *
     * @param context The context for this component.
     * @param uiInfo An object providing initial values for the bottom sheet model.
     * @param skipLoadingForFixFlow When true, loading is skipped due to the fix flow.
     * @param bottomSheetController The bottom sheet controller where this bottom sheet will be
     *     shown.
     * @param layoutStateProvider The LayoutStateProvider used to detect when the bottom sheet needs
     *     to be hidden after a change of layout (e.g. to the tab switcher).
     * @param tabModel The TabModel used to detect when the bottom sheet needs to be hidden after a
     *     tab change.
     * @param delegate The native callbacks for user actions.
     */
    public AutofillSaveCardBottomSheetCoordinator(
            Context context,
            AutofillSaveCardUiInfo uiInfo,
            boolean skipLoadingForFixFlow,
            BottomSheetController bottomSheetController,
            LayoutStateProvider layoutStateProvider,
            TabModel tabModel,
            NativeDelegate delegate) {
        mContext = context;
        mView = new AutofillSaveCardBottomSheetView(context);

        mModel =
                new PropertyModel.Builder(AutofillSaveCardBottomSheetProperties.ALL_KEYS)
                        .with(AutofillSaveCardBottomSheetProperties.TITLE, uiInfo.getTitleText())
                        .with(
                                AutofillSaveCardBottomSheetProperties.DESCRIPTION,
                                uiInfo.getDescriptionText())
                        .with(
                                AutofillSaveCardBottomSheetProperties.LOGO_ICON,
                                uiInfo.isForUpload() ? uiInfo.getLogoIcon() : 0)
                        .with(
                                AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION,
                                uiInfo.getCardDescription())
                        .with(
                                AutofillSaveCardBottomSheetProperties.CARD_ICON,
                                uiInfo.getCardDetail().issuerIconDrawableId)
                        .with(
                                AutofillSaveCardBottomSheetProperties.CARD_LABEL,
                                uiInfo.getCardDetail().label)
                        .with(
                                AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL,
                                uiInfo.getCardDetail().subLabel)
                        .with(
                                AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                                new AutofillSaveCardBottomSheetProperties.LegalMessage(
                                        uiInfo.getLegalMessageLines(), this::openLegalMessageLink))
                        .with(
                                AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL,
                                uiInfo.getConfirmText())
                        .with(
                                AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                uiInfo.getCancelText())
                        .with(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE, false)
                        .with(
                                AutofillSaveCardBottomSheetProperties.LOADING_DESCRIPTION,
                                uiInfo.getLoadingDescription())
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, mView, AutofillSaveCardBottomSheetViewBinder::bind);

        mMediator =
                new AutofillSaveCardBottomSheetMediator(
                        new AutofillSaveCardBottomSheetContent(
                                mView.mContentView, mView.mScrollView),
                        new AutofillSaveCardBottomSheetLifecycle(
                                bottomSheetController, layoutStateProvider, tabModel),
                        bottomSheetController,
                        mModel,
                        delegate,
                        uiInfo.isForUpload(),
                        skipLoadingForFixFlow);

        mView.mAcceptButton.setOnClickListener(
                (View button) -> {
                    mMediator.onAccepted();
                });
        mView.mCancelButton.setOnClickListener(
                (View button) -> {
                    mMediator.onCanceled();
                });
    }

    /**
     * Request to show the bottom sheet.
     *
     * <p>Calls {@link AutofillSaveCardBottomSheetBridge#onUiShown} if the bottom sheet was shown.
     */
    public void requestShowContent() {
        mMediator.requestShowContent();
    }

    /** Hides this component hiding the bottom sheet if needed. */
    public void hide(@StateChangeReason int hideReason) {
        mMediator.hide(hideReason);
    }

    AutofillSaveCardBottomSheetView getAutofillSaveCardBottomSheetViewForTesting() {
        return mView;
    }

    PropertyModel getPropertyModelForTesting() {
        return mModel;
    }

    void openLegalMessageLink(String url) {
        new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(mContext, Uri.parse(url));
    }
}
