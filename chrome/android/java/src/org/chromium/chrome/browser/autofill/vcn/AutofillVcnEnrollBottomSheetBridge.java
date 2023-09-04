// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.layouts.LayoutManagerProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.LinkedList;

/** Bridge for the virtual card enrollment bottom sheet. */
@JNINamespace("autofill")
/*package*/ class AutofillVcnEnrollBottomSheetBridge
        implements AutofillVcnEnrollBottomSheetCoordinator.Delegate {
    private long mNativeAutofillVcnEnrollBottomSheetBridge;
    private AutofillVcnEnrollBottomSheetCoordinator mCoordinator;

    private LayoutStateProvider mLayoutStateProviderForTesting;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplierForTesting;

    @CalledByNative
    @VisibleForTesting
    /*package*/ AutofillVcnEnrollBottomSheetBridge() {}

    /**
     * Requests to show the bottom sheet. Called via JNI from C++.
     *
     * @param nativeAutofillVcnEnrollBottomSheetBridge The native pointer to the C++ bridge that
     *                                                 receives callbacks.
     * @param webContents The web contents where the bottom sheet should show.
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
     *
     * @return True if shown.
     */
    @CalledByNative
    @VisibleForTesting
    /*package*/ boolean requestShowContent(long nativeAutofillVcnEnrollBottomSheetBridge,
            WebContents webContents, String messageText, String descriptionText,
            String learnMoreLinkText, String cardContainerAccessibilityDescription,
            Bitmap issuerIcon, String cardLabel, String cardDescription,
            LinkedList<LegalMessageLine> googleLegalMessages,
            LinkedList<LegalMessageLine> issuerLegalMessages, String acceptButtonLabel,
            String cancelButtonLabel) {
        if (webContents == null || webContents.isDestroyed()) return false;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return false;

        if (mNativeAutofillVcnEnrollBottomSheetBridge != 0) return false;
        mNativeAutofillVcnEnrollBottomSheetBridge = nativeAutofillVcnEnrollBottomSheetBridge;

        ArrayList<String> descriptionTextComponents = new ArrayList<>();
        descriptionTextComponents.add(descriptionText);
        descriptionTextComponents.add(learnMoreLinkText);
        descriptionTextComponents.add(
                ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL);

        PropertyModel.Builder modelBuilder =
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
                                cancelButtonLabel);

        mCoordinator = new AutofillVcnEnrollBottomSheetCoordinator(window.getContext().get(),
                modelBuilder,
                mLayoutStateProviderForTesting != null ? mLayoutStateProviderForTesting
                                                       : LayoutManagerProvider.from(window),
                mTabModelSelectorSupplierForTesting != null ? mTabModelSelectorSupplierForTesting
                                                            : TabModelSelectorSupplier.from(window),
                /*delegate=*/this);

        return mCoordinator.requestShowContent(window);
    }

    /*package*/ void setLayoutStateProviderForTesting(LayoutStateProvider layoutStateProvider) {
        mLayoutStateProviderForTesting = layoutStateProvider;
    }

    /*package*/ void setTabModelSelectorSupplierForTesting(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplierForTesting = tabModelSelectorSupplier;
    }

    // AutofillVcnEnrollBottomSheetDelegate:
    @Override
    @VisibleForTesting
    public void onAccept() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onAccept(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    // AutofillVcnEnrollBottomSheetDelegate:
    @Override
    @VisibleForTesting
    public void onCancel() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onCancel(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    // AutofillVcnEnrollBottomSheetDelegate:
    @Override
    @VisibleForTesting
    public void onDismiss() {
        if (mNativeAutofillVcnEnrollBottomSheetBridge == 0) return;
        AutofillVcnEnrollBottomSheetBridgeJni.get().onDismiss(
                mNativeAutofillVcnEnrollBottomSheetBridge);
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
    }

    @CalledByNative
    @VisibleForTesting
    /*package*/ void hide() {
        mNativeAutofillVcnEnrollBottomSheetBridge = 0;
        if (mCoordinator == null) return;
        mCoordinator.hide();
        mCoordinator = null;
    }

    @NativeMethods
    interface Natives {
        void onAccept(long nativeAutofillVCNEnrollBottomSheetBridge);
        void onCancel(long nativeAutofillVCNEnrollBottomSheetBridge);
        void onDismiss(long nativeAutofillVCNEnrollBottomSheetBridge);
    }
}
