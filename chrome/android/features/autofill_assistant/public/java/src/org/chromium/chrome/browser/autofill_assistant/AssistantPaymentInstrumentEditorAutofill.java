// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.settings.AddressEditor;
import org.chromium.chrome.browser.autofill.settings.CardEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantEditor.AssistantPaymentInstrumentEditor;
import org.chromium.components.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Editor for credit cards in Chrome using Autofill as a base.
 */
public class AssistantPaymentInstrumentEditorAutofill implements AssistantPaymentInstrumentEditor {
    private static final String BASIC_CARD = "basic-card";

    private final WebContents mWebContents;
    private final Context mContext;
    private final CardEditor mEditor;
    private final AddressEditor mAddressEditor;

    public AssistantPaymentInstrumentEditorAutofill(WebContents webContents, Activity activity,
            List<String> supportedCardNetworks, boolean shouldStoreChanges) {
        mWebContents = webContents;
        mContext = activity;

        Profile profile = Profile.fromWebContents(webContents);

        mAddressEditor = new AddressEditor(AddressEditor.Purpose.AUTOFILL_ASSISTANT,
                /* saveToDisk= */ shouldStoreChanges);
        mAddressEditor.setEditorDialog(new EditorDialog(activity,
                /* deleteRunnable= */ null, profile));

        mEditor = new CardEditor(webContents, mAddressEditor,
                /* includeOrgLabel= */ false, /* saveToDisk= */ shouldStoreChanges);
        EditorDialog cardEditorDialog = new EditorDialog(activity,
                /*deleteRunnable =*/null, profile);
        if (VersionInfo.isBetaBuild() || VersionInfo.isStableBuild()) {
            cardEditorDialog.disableScreenshots();
        }
        mEditor.setEditorDialog(cardEditorDialog);

        mEditor.addAcceptedPaymentMethodIfRecognized(
                getPaymentMethodDataFromNetworks(supportedCardNetworks));
    }

    private PaymentMethodData getPaymentMethodDataFromNetworks(List<String> supportedCardNetworks) {
        // Only enable 'basic-card' payment method.
        PaymentMethodData methodData = new PaymentMethodData();
        methodData.supportedMethod = BASIC_CARD;

        // Apply basic-card filter if specified
        if (!supportedCardNetworks.isEmpty()) {
            ArrayList<Integer> filteredNetworks = new ArrayList<>();
            Map<String, Integer> networks = getNetworkIdentifiers();
            for (String network : supportedCardNetworks) {
                assert networks.containsKey(network);
                if (networks.containsKey(network)) {
                    filteredNetworks.add(networks.get(network));
                }
            }

            methodData.supportedNetworks = new int[filteredNetworks.size()];
            for (int i = 0; i < filteredNetworks.size(); ++i) {
                methodData.supportedNetworks[i] = filteredNetworks.get(i);
            }
        }

        return methodData;
    }

    private static Map<String, Integer> getNetworkIdentifiers() {
        Map<String, Integer> networks = new HashMap<>();
        networks.put("amex", BasicCardNetwork.AMEX);
        networks.put("diners", BasicCardNetwork.DINERS);
        networks.put("discover", BasicCardNetwork.DISCOVER);
        networks.put("jcb", BasicCardNetwork.JCB);
        networks.put("mastercard", BasicCardNetwork.MASTERCARD);
        networks.put("mir", BasicCardNetwork.MIR);
        networks.put("unionpay", BasicCardNetwork.UNIONPAY);
        networks.put("visa", BasicCardNetwork.VISA);
        return networks;
    }

    @Override
    public void createOrEditItem(@Nullable PaymentInstrumentModel oldItem,
            Callback<PaymentInstrumentModel> doneCallback,
            Callback<PaymentInstrumentModel> cancelCallback) {
        @Nullable
        AutofillPaymentInstrument paymentInstrument = oldItem == null
                ? null
                : AssistantAutofillUtilChrome.assistantPaymentInstrumentToAutofillPaymentInstrument(
                        oldItem.mOption, mWebContents);

        Callback<AutofillPaymentInstrument> editorDoneCallback = editedPaymentInstrument -> {
            assert (editedPaymentInstrument != null && editedPaymentInstrument.isComplete()
                    && editedPaymentInstrument.getCard() != null);
            if (TextUtils.isEmpty(editedPaymentInstrument.getCard().getGUID())) {
                // b/231645674: In the case where the card is not stored, it will come without GUID.
                // Combined with the missing "update" signal from the PersonalDataManager this
                // causes our UI to show the card twice.
                editedPaymentInstrument.getCard().setGUID(UUID.randomUUID().toString());
            }
            doneCallback.onResult(new PaymentInstrumentModel(
                    AssistantAutofillUtilChrome
                            .autofillPaymentInstrumentToAssistantPaymentInstrument(
                                    editedPaymentInstrument)));
        };

        Callback<AutofillPaymentInstrument> editorCancelCallback =
                editedPaymentInstrument -> cancelCallback.onResult(oldItem);

        mEditor.edit(paymentInstrument, editorDoneCallback, editorCancelCallback);
    }

    /**
     * Adds the address's information to the editor, such that it can be selected as a billing
     * address for the card.
     *
     * @param address The {@link AssistantAutofillProfile} to add information for.
     */
    @Override
    public void addAddressInformationForAutocomplete(AssistantAutofillProfile address) {
        mAddressEditor.addPhoneNumberIfValid(address.getPhoneNumber());

        AutofillAddress autofillAddress =
                AssistantAutofillUtilChrome.assistantAutofillProfileToAutofillAddress(
                        address, mContext);
        if (autofillAddress.getProfile().getLabel() == null) {
            autofillAddress.getProfile().setLabel(
                    PersonalDataManager.getInstance().getBillingAddressLabelForPaymentRequest(
                            autofillAddress.getProfile()));
        }
        mEditor.updateBillingAddressIfComplete(autofillAddress);
    }
}
