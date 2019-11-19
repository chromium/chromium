// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.payments.mojom.BasicCardNetwork;
import org.chromium.payments.mojom.BasicCardType;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Basic-card utils */
public class BasicCardUtils {
    /** The total number of all possible card types (i.e., credit, debit, prepaid, unknown). */
    public static final int TOTAL_NUMBER_OF_CARD_TYPES = 4;

    /**
     * @return A set of card networks (e.g., "visa", "amex") accepted by the "basic-card" payment
     *         method data.
     */
    public static Set<String> convertBasicCardToNetworks(PaymentMethodData data) {
        assert data != null;

        Map<Integer, String> networks = getNetworks();
        if (!isBasicCardNetworkSpecified(data)) {
            // Not specified indicates support of all issuer networks.
            return new HashSet<>(networks.values());
        }

        // Supports some issuer networks.
        Set<String> result = new HashSet<>();
        for (int i = 0; i < data.supportedNetworks.length; i++) {
            String network = networks.get(data.supportedNetworks[i]);
            if (network != null) result.add(network);
        }
        return result;
    }

    /**
     * @return A set of card types (e.g., CardType.DEBIT, CardType.PREPAID)
     *         accepted by the "basic-card" payment method data.
     */
    public static Set<Integer> convertBasicCardToTypes(PaymentMethodData data) {
        assert data != null;

        Set<Integer> result = new HashSet<>();
        result.add(CardType.UNKNOWN);

        Map<Integer, Integer> cardTypes = getCardTypes();
        if (!isBasicCardTypeSpecified(data)) {
            // Not specified indicates support of all card types.
            result.addAll(cardTypes.values());
        } else {
            // Supports some card types.
            for (int i = 0; i < data.supportedTypes.length; i++) {
                Integer cardType = cardTypes.get(data.supportedTypes[i]);
                if (cardType != null) result.add(cardType);
            }
        }

        return result;
    }

    /**
     * @return True if supported card type is specified for the "basic-card" payment method data.
     */
    public static boolean isBasicCardTypeSpecified(PaymentMethodData data) {
        assert data != null;

        return data.supportedTypes != null && data.supportedTypes.length != 0;
    }

    /**
     * @return True if supported card network is specified for "basic-card" payment method data.
     */
    public static boolean isBasicCardNetworkSpecified(PaymentMethodData data) {
        assert data != null;

        return data.supportedNetworks != null && data.supportedNetworks.length != 0;
    }

    /**
     * @return a complete map of BasicCardNetworks to strings.
     */
    public static Map<Integer, String> getNetworks() {
        Map<Integer, String> networks = new HashMap<>();
        networks.put(BasicCardNetwork.AMEX, "amex");
        networks.put(BasicCardNetwork.DINERS, "diners");
        networks.put(BasicCardNetwork.DISCOVER, "discover");
        networks.put(BasicCardNetwork.JCB, "jcb");
        networks.put(BasicCardNetwork.MASTERCARD, "mastercard");
        networks.put(BasicCardNetwork.MIR, "mir");
        networks.put(BasicCardNetwork.UNIONPAY, "unionpay");
        networks.put(BasicCardNetwork.VISA, "visa");
        return networks;
    }

    /**
     * @return a complete map of string identifiers to BasicCardNetworks.
     */
    public static Map<String, Integer> getNetworkIdentifiers() {
        Map<Integer, String> networksByInt = getNetworks();
        Map<String, Integer> networksByString = new HashMap<>();
        for (Map.Entry<Integer, String> entry : networksByInt.entrySet()) {
            networksByString.put(entry.getValue(), entry.getKey());
        }
        return networksByString;
    }

    /**
     * @return a complete map of BasicCardType to CardType.
     */
    public static Map<Integer, Integer> getCardTypes() {
        Map<Integer, Integer> cardTypes = new HashMap<>();
        cardTypes.put(BasicCardType.CREDIT, CardType.CREDIT);
        cardTypes.put(BasicCardType.DEBIT, CardType.DEBIT);
        cardTypes.put(BasicCardType.PREPAID, CardType.PREPAID);
        return cardTypes;
    }

    private BasicCardUtils() {}
}
