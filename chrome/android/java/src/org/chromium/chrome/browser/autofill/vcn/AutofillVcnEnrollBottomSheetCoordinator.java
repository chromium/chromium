// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.LinkedList;

/** Coordinator controller for the virtual card enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetCoordinator {
    private final AutofillVcnEnrollBottomSheetMediator mMediator;

    /**
     * Constructs a coordinator controller for the virtual card enrollment bottom sheet.
     *
     * @param context The activity context.
     * @param messageText The prompt message for the bottom sheet, e.g., "Make it more secure with a
     *                    virtual card next time?"
     * @param descriptionText A text that describes what a virtual card does, e.g., "A virtual card
     *                        hides your actual card..." and so on. This text includes a "learn
     *                        more" link text.
     * @param learnMoreLinkText The text of the "learn more" link in descriptionText.
     * @param cardContainerAccessibilityDescription The accessibility description for the UI element
     *                                              that contains the issuer icon, card label, and
     *                                              card description.
     * @param issuerIcon The icon for the card. For example, could be an American Express logo.
     * @param cardLabel The label for the card, e.g., "Amex ****1234".
     * @param cardDescription The description of the card, e.g., "Virtual Card".
     * @param googleLegalMessages Legal messages from Google Pay.
     * @param issuerLegalMessages Legal messages from the issuer bank.
     * @param acceptButtonLabel The label for the button that enrolls a virtual card.
     * @param cancelButtonLabel The label for the button that cancels enrollment.
     * @param onAccept The callback to invoke when the user accepts the enrollment prompt.
     * @param onCancel The callback to invoke when the user cancels the enrollment prompt.
     * @param onDismiss The callback to invoke when the user dismisses the bottom sheet.
     */
    /*package*/ AutofillVcnEnrollBottomSheetCoordinator(Context context, String messageText,
            String descriptionText, String learnMoreLinkText,
            String cardContainerAccessibilityDescription, Bitmap issuerIcon, String cardLabel,
            String cardDescription, LinkedList<LegalMessageLine> googleLegalMessages,
            LinkedList<LegalMessageLine> issuerLegalMessages, String acceptButtonLabel,
            String cancelButtonLabel, Runnable onAccept, Runnable onCancel, Runnable onDismiss) {
        ArrayList<String> descriptionTextComponents = new ArrayList<>();
        descriptionTextComponents.add(descriptionText);
        descriptionTextComponents.add(learnMoreLinkText);
        descriptionTextComponents.add(
                ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL);

        PropertyModel model =
                new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS)
                        .with(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, messageText)
                        .with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION_TEXT,
                                descriptionTextComponents)
                        .with(AutofillVcnEnrollBottomSheetProperties
                                        .CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION,
                                cardContainerAccessibilityDescription)
                        .with(AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON, issuerIcon)
                        .with(AutofillVcnEnrollBottomSheetProperties.CARD_LABEL, cardLabel)
                        .with(AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION,
                                cardDescription)
                        .with(AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES,
                                googleLegalMessages)
                        .with(AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES,
                                issuerLegalMessages)
                        .with(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL,
                                acceptButtonLabel)
                        .with(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                cancelButtonLabel)
                        .build();

        AutofillVcnEnrollBottomSheetView view = new AutofillVcnEnrollBottomSheetView(context);

        AutofillUiUtils.CardIconSpecs cardIconSpecs =
                AutofillUiUtils.CardIconSpecs.create(context, AutofillUiUtils.CardIconSize.LARGE);
        AutofillVcnEnrollBottomSheetViewBinder viewBinder =
                new AutofillVcnEnrollBottomSheetViewBinder(this::launchChromeCustomTab,
                        cardIconSpecs.getWidth(), cardIconSpecs.getHeight());
        PropertyModelChangeProcessor.create(model, view, viewBinder::bind);

        mMediator = new AutofillVcnEnrollBottomSheetMediator(view.mContentView, view.mScrollView,
                view.mAcceptButton, view.mCancelButton, onAccept, onCancel, onDismiss);
    }

    private void launchChromeCustomTab(String url) {
        new CustomTabsIntent.Builder().setShowTitle(true).build().launchUrl(
                mMediator.getContentView().getContext(), Uri.parse(url));
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    /*package*/ boolean requestShowContent(WindowAndroid window) {
        return mMediator.requestShowContent(window);
    }

    /** Hides the virtual card enrollment bottom sheet, if present. */
    /*package*/ void hide() {
        mMediator.hide();
    }
}
