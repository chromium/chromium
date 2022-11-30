// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.PriceTrackableOffer;

import java.util.Locale;

/**
 * Helpers for serializing and deserializing {@link CommerceSubscription} objects.
 */
class CommerceSubscriptionJsonSerializer {
    private static final String TAG = "CSJS";
    private static final String SUBSCRIPTION_TYPE_KEY = "type";
    private static final String SUBSCRIPTION_IDENTIFIER_KEY = "identifier";
    private static final String SUBSCRIPTION_IDENTIFIER_TYPE_KEY = "identifierType";
    private static final String SUBSCRIPTION_MANAGEMENT_TYPE_KEY = "managementType";
    private static final String SUBSCRIPTION_TIMESTAMP_KEY = "eventTimestampMicros";
    private static final String SUBSCRIPTION_SEEN_OFFER_KEY = "userSeenOffer";
    private static final String SEEN_OFFER_ID_KEY = "offerId";
    private static final String SEEN_OFFER_PRICE_KEY = "seenPriceMicros";
    private static final String SEEN_OFFER_COUNTRY_KEY = "countryCode";

    /** Creates a {@link CommerceSubscription} from a {@link JSONObject}. */
    public static CommerceSubscription deserialize(JSONObject json) {
        try {
            return new CommerceSubscription(json.getString(SUBSCRIPTION_TYPE_KEY),
                    json.getString(SUBSCRIPTION_IDENTIFIER_KEY),
                    json.getString(SUBSCRIPTION_MANAGEMENT_TYPE_KEY),
                    json.getString(SUBSCRIPTION_IDENTIFIER_TYPE_KEY),
                    Long.parseLong(json.getString(SUBSCRIPTION_TIMESTAMP_KEY)));

        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to deserialize CommerceSubscription. Details: %s",
                            e.getMessage()));
        }
        return null;
    }

    /** Creates a {@link JSONObject}from a  {@link CommerceSubscription}. */
    public static JSONObject serialize(CommerceSubscription subscription) {
        try {
            JSONObject subscriptionJson = new JSONObject();
            subscriptionJson.put(SUBSCRIPTION_TYPE_KEY, subscription.getType());
            subscriptionJson.put(
                    SUBSCRIPTION_MANAGEMENT_TYPE_KEY, subscription.getManagementType());
            subscriptionJson.put(
                    SUBSCRIPTION_IDENTIFIER_TYPE_KEY, subscription.getTrackingIdType());
            subscriptionJson.put(SUBSCRIPTION_IDENTIFIER_KEY, subscription.getTrackingId());

            PriceTrackableOffer seenOffer = subscription.getSeenOffer();
            if (CommerceSubscriptionsServiceConfig.shouldParseSeenOfferToServer()
                    && seenOffer != null) {
                JSONObject seenOfferJson = new JSONObject();
                seenOfferJson.put(SEEN_OFFER_ID_KEY, seenOffer.offerId);
                seenOfferJson.put(SEEN_OFFER_PRICE_KEY, seenOffer.currentPrice);
                seenOfferJson.put(SEEN_OFFER_COUNTRY_KEY, seenOffer.countryCode);
                subscriptionJson.put(SUBSCRIPTION_SEEN_OFFER_KEY, seenOfferJson);
            }

            return subscriptionJson;
        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to serialize CommerceSubscription. Details: %s",
                            e.getMessage()));
        }

        return null;
    }
}
