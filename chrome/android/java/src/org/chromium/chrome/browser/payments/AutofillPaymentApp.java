// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Provides access to locally stored user credit cards.
 */
public class AutofillPaymentApp implements PaymentApp {
    private final WebContents mWebContents;
    private Set<Integer> mBasicCardTypes;
    private Set<String> mBasicCardSupportedNetworks;
    private Set<String> mSupportedMethods;

    /**
     * Builds a payment app backed by autofill cards.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     */
    public AutofillPaymentApp(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public void getInstruments(String unusedId, Map<String, PaymentMethodData> methodDataMap,
            String unusedOrigin, String unusedIFRameOrigin, byte[][] unusedCertificateChain,
            Map<String, PaymentDetailsModifier> unusedModifiers,
            final InstrumentsCallback callback) {
        new Handler().post(
                ()
                        -> callback.onInstrumentsReady(AutofillPaymentApp.this,
                                getInstruments(methodDataMap, /*forceReturnServerCards=*/false)));
    }

    /** Method to get instruments synchronously. */
    public List<PaymentInstrument> getInstruments(
            Map<String, PaymentMethodData> methodDataMap, boolean forceReturnServerCards) {
        PersonalDataManager pdm = PersonalDataManager.getInstance();
        List<CreditCard> cards = pdm.getCreditCardsToSuggest(
                /*includeServerCards=*/forceReturnServerCards
                || ChromeFeatureList.isEnabled(
                           ChromeFeatureList.WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD));
        List<PaymentInstrument> instruments = new ArrayList<>(cards.size());

        if (methodDataMap.containsKey(MethodStrings.BASIC_CARD)) {
            mBasicCardSupportedNetworks = BasicCardUtils.convertBasicCardToNetworks(
                    methodDataMap.get(MethodStrings.BASIC_CARD));
            mBasicCardTypes = BasicCardUtils.convertBasicCardToTypes(
                    methodDataMap.get(MethodStrings.BASIC_CARD));
        } else {
            mBasicCardTypes = new HashSet<>(BasicCardUtils.getCardTypes().values());
            mBasicCardTypes.add(CardType.UNKNOWN);
        }
        mSupportedMethods = new HashSet<>(methodDataMap.keySet());

        for (int i = 0; i < cards.size(); i++) {
            PaymentInstrument instrument = getInstrumentForCard(cards.get(i));
            if (instrument != null) instruments.add(instrument);
        }

        return instruments;
    }

    /**
     * Creates a payment instrument object for the given card if it is usable for the
     * payment request. This interface must be called after getInstruments.
     *
     * @param card The given card.
     */
    @Nullable
    public PaymentInstrument getInstrumentForCard(CreditCard card) {
        if (mSupportedMethods == null) return null;

        PersonalDataManager pdm = PersonalDataManager.getInstance();
        AutofillProfile billingAddress = TextUtils.isEmpty(card.getBillingAddressId())
                ? null
                : pdm.getProfile(card.getBillingAddressId());

        if (billingAddress != null
                && AutofillAddress.checkAddressCompletionStatus(
                           billingAddress, AutofillAddress.CompletenessCheckType.IGNORE_PHONE)
                        != AutofillAddress.CompletionStatus.COMPLETE) {
            billingAddress = null;
        }

        if (billingAddress == null) card.setBillingAddressId(null);

        String methodName = null;
        if (mBasicCardSupportedNetworks != null
                && mBasicCardSupportedNetworks.contains(card.getBasicCardIssuerNetwork())) {
            methodName = MethodStrings.BASIC_CARD;
        } else if (mSupportedMethods.contains(card.getBasicCardIssuerNetwork())) {
            methodName = card.getBasicCardIssuerNetwork();
        }

        if (methodName != null && mBasicCardTypes.contains(card.getCardType())) {
            // Whether this card matches the card type (credit, debit, prepaid) exactly. If the
            // merchant requests all card types, then this is always true. If the merchant
            // requests only a subset of card types, then this is false for "unknown" card
            // types. The "unknown" card types is where Chrome is unable to determine the type
            // of card. Cards that don't match the card type exactly cannot be pre-selected in
            // the UI.
            boolean matchesMerchantCardTypeExactly = card.getCardType() != CardType.UNKNOWN
                    || mBasicCardTypes.size() == BasicCardUtils.TOTAL_NUMBER_OF_CARD_TYPES;

            return new AutofillPaymentInstrument(
                    mWebContents, card, billingAddress, methodName, matchesMerchantCardTypeExactly);
        }

        return null;
    }

    @Override
    public Set<String> getAppMethodNames() {
        Set<String> methods = new HashSet<>(BasicCardUtils.getNetworks().values());
        methods.add(MethodStrings.BASIC_CARD);
        return methods;
    }

    @Override
    public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodDataMap) {
        return merchantSupportsAutofillPaymentInstruments(methodDataMap);
    }

    /** @return True if the merchant methodDataMap supports autofill payment instruments. */
    public static boolean merchantSupportsAutofillPaymentInstruments(
            Map<String, PaymentMethodData> methodDataMap) {
        assert methodDataMap != null;
        PaymentMethodData basicCardData = methodDataMap.get(MethodStrings.BASIC_CARD);
        if (basicCardData != null) {
            Set<String> basicCardNetworks =
                    BasicCardUtils.convertBasicCardToNetworks(basicCardData);
            if (basicCardNetworks != null && !basicCardNetworks.isEmpty()) return true;
        }

        Set<String> methodNames = new HashSet<>(methodDataMap.keySet());
        methodNames.retainAll(BasicCardUtils.getNetworks().values());
        return !methodNames.isEmpty();
    }

    @Override
    public String getAppIdentifier() {
        return "Chrome_Autofill_Payment_App";
    }

    @Override
    public int getAdditionalAppTextResourceId() {
        // If the merchant has restricted the accepted card types (credit, debit, prepaid), then the
        // list of payment instruments should include a message describing the accepted card types,
        // e.g., "Debit cards are accepted" or "Debit and prepaid cards are accepted."
        if (mBasicCardTypes == null
                || mBasicCardTypes.size() == BasicCardUtils.TOTAL_NUMBER_OF_CARD_TYPES) {
            return 0;
        }

        int credit = mBasicCardTypes.contains(CardType.CREDIT) ? 1 : 0;
        int debit = mBasicCardTypes.contains(CardType.DEBIT) ? 1 : 0;
        int prepaid = mBasicCardTypes.contains(CardType.PREPAID) ? 1 : 0;
        int[][][] resourceIds = new int[2][2][2];
        resourceIds[0][0][0] = 0;
        resourceIds[0][0][1] = R.string.payments_prepaid_cards_are_accepted_label;
        resourceIds[0][1][0] = R.string.payments_debit_cards_are_accepted_label;
        resourceIds[0][1][1] = R.string.payments_debit_prepaid_cards_are_accepted_label;
        resourceIds[1][0][0] = R.string.payments_credit_cards_are_accepted_label;
        resourceIds[1][0][1] = R.string.payments_credit_prepaid_cards_are_accepted_label;
        resourceIds[1][1][0] = R.string.payments_credit_debit_cards_are_accepted_label;
        resourceIds[1][1][1] = 0;
        return resourceIds[credit][debit][prepaid];
    }
}
