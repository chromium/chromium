// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Wrapper around CommerceSubscriptions Web APIs.
 */
public final class CommerceSubscriptionsServiceProxy {
    private static final String TAG = "CSSP";
    private static final long HTTPS_REQUEST_TIMEOUT_MS = 1000L;
    private static final String GET_HTTPS_METHOD = "GET";
    private static final String POST_HTTPS_METHOD = "POST";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EMPTY_POST_DATA = "";
    private static final String[] OAUTH_SCOPE =
            new String[] {"https://www.googleapis.com/auth/chromememex"};
    private static final String OAUTH_NAME = "susbcriptions_svc";
    private static final String STATUS_KEY = "status";
    private static final String STATUS_CODE_KEY = "code";
    private static final String REMOVE_SUBSCRIPTIONS_REQUEST_PARAMS_KEY =
            "removeShoppingSubscriptionsParams";
    private static final String CREATE_SUBSCRIPTIONS_REQUEST_PARAMS_KEY =
            "createShoppingSubscriptionsParams";
    private static final String EVENT_TIMESTAMP_MICROS_KEY = "eventTimestampMicros";
    private static final String SUBSCRIPTIONS_KEY = "subscriptions";
    private static final String GET_SUBSCRIPTIONS_QUERY_PARAMS_TEMPLATE =
            "?requestParams.subscriptionType=%s";
    private static final int BACKEND_CANONICAL_CODE_SUCCESS = 0;

    /**
     * Makes an HTTPS call to the backend in order to create the provided subscriptions.
     * @param subscriptions list of {@link CommerceSubscription} to create.
     * @param callback indicates whether or not the operation succeeded on the backend.
     */
    public void create(List<CommerceSubscription> subscriptions, Callback<Boolean> callback) {
        manageSubscriptions(getCreateSubscriptionsRequestParams(subscriptions), callback);
    }

    /**
     * Makes an HTTPS call to the backend to delete the provided list of subscriptions.
     * @param subscriptions list of {@link CommerceSubscription} to delete.
     * @param callback indicates whether or not the operation succeeded on the backend.
     */
    public void delete(List<CommerceSubscription> subscriptions, Callback<Boolean> callback) {
        manageSubscriptions(getRemoveSubscriptionsRequestParams(subscriptions), callback);
    }

    /**
     * Fetches all subscriptions that match the provided type from the backend.
     * @param type the type of subscriptions to fetch.
     * @param callback contains the list of subscriptions returned from the server.
     */
    public void get(@CommerceSubscription.CommerceSubscriptionType String type,
            Callback<List<CommerceSubscription>> callback) {
        // TODO(crbug.com/1195469) Accept Profile instance from SubscriptionsManager.
        EndpointFetcher.fetchUsingOAuth(
                (response)
                        -> {
                    callback.onResult(createCommerceSubscriptions(response.getResponseString()));
                },
                Profile.getLastUsedRegularProfile(), OAUTH_NAME,
                CommerceSubscriptionsServiceConfig.SUBSCRIPTIONS_SERVICE_BASE_URL.getValue()
                        + String.format(GET_SUBSCRIPTIONS_QUERY_PARAMS_TEMPLATE, type),
                GET_HTTPS_METHOD, CONTENT_TYPE, OAUTH_SCOPE, EMPTY_POST_DATA,
                HTTPS_REQUEST_TIMEOUT_MS);
    }

    private void manageSubscriptions(JSONObject requestPayload, Callback<Boolean> callback) {
        // TODO(crbug.com/1195469) Accept Profile instance from SubscriptionsManager.
        EndpointFetcher.fetchUsingOAuth(
                (response)
                        -> {
                    callback.onResult(
                            didManageSubscriptionCallSucceed(response.getResponseString()));
                },
                Profile.getLastUsedRegularProfile(), OAUTH_NAME,
                CommerceSubscriptionsServiceConfig.SUBSCRIPTIONS_SERVICE_BASE_URL.getValue(),
                POST_HTTPS_METHOD, CONTENT_TYPE, OAUTH_SCOPE, requestPayload.toString(),
                HTTPS_REQUEST_TIMEOUT_MS);
    }

    private boolean didManageSubscriptionCallSucceed(String responseString) {
        try {
            JSONObject response = new JSONObject(responseString);
            JSONObject statusJson = response.getJSONObject(STATUS_KEY);
            int statusCode = statusJson.getInt(STATUS_CODE_KEY);
            return statusCode == BACKEND_CANONICAL_CODE_SUCCESS;
        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to create CreateSubscriptionRequestParams. Details: %s",
                            e.getMessage()));
        }

        return false;
    }

    private JSONObject getCreateSubscriptionsRequestParams(
            List<CommerceSubscription> subscriptions) {
        JSONObject container = new JSONObject();
        JSONArray subscriptionsJsonArray = new JSONArray();
        try {
            for (CommerceSubscription subscription : subscriptions) {
                subscriptionsJsonArray.put(
                        CommerceSubscriptionJsonSerializer.serialize(subscription));
            }

            JSONObject subscriptionsObject = new JSONObject();
            subscriptionsObject.put(SUBSCRIPTIONS_KEY, subscriptionsJsonArray);

            container.put(CREATE_SUBSCRIPTIONS_REQUEST_PARAMS_KEY, subscriptionsObject);
        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to create CreateSubscriptionRequestParams. Details: %s",
                            e.getMessage()));
        }

        return container;
    }

    private JSONObject getRemoveSubscriptionsRequestParams(
            List<CommerceSubscription> subscriptions) {
        JSONObject container = new JSONObject();
        try {
            JSONObject removeSubscriptionsParamsJson = new JSONObject();
            JSONArray subscriptionsTimestamps = new JSONArray();
            for (CommerceSubscription subscription : subscriptions) {
                if (subscription.getTimestamp() == CommerceSubscription.UNSAVED_SUBSCRIPTION) {
                    continue;
                }
                subscriptionsTimestamps.put(subscription.getTimestamp());
            }
            removeSubscriptionsParamsJson.put(EVENT_TIMESTAMP_MICROS_KEY, subscriptionsTimestamps);
            container.put(REMOVE_SUBSCRIPTIONS_REQUEST_PARAMS_KEY, removeSubscriptionsParamsJson);
        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to create RemoveSubscriptionsRequestParams. Details: %s",
                            e.getMessage()));
        }

        return container;
    }

    private List<CommerceSubscription> createCommerceSubscriptions(String responseString) {
        List<CommerceSubscription> subscriptions = new ArrayList<>();
        try {
            JSONObject response = new JSONObject(responseString);
            JSONArray subscriptionsJsonArray = response.getJSONArray(SUBSCRIPTIONS_KEY);

            for (int i = 0; i < subscriptionsJsonArray.length(); i++) {
                JSONObject subscriptionJson = subscriptionsJsonArray.getJSONObject(i);
                CommerceSubscription subscription =
                        CommerceSubscriptionJsonSerializer.deserialize(subscriptionJson);
                if (subscription != null) {
                    subscriptions.add(subscription);
                }
            }
        } catch (JSONException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "Failed to deserialize Subscriptions list. Details: %s",
                            e.getMessage()));
        }

        return subscriptions;
    }
}
