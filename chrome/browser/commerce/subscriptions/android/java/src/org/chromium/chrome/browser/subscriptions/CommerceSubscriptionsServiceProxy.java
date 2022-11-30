// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
/**
 * Wrapper around CommerceSubscriptions Web APIs.
 */
public class CommerceSubscriptionsServiceProxy {
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

    // TODO(crbug.com/1311754): These parameters (url, OAUTH_SCOPE, etc.) are copied from
    // web_history_service.cc directly, it works now but we should figure out a better way to
    // keep these parameters in sync.
    private static final String WAA_QUERY_URL =
            "https://history.google.com/history/api/lookup?client=web_app";
    private static final String[] WAA_OAUTH_SCOPE =
            new String[] {"https://www.googleapis.com/auth/chromesync"};
    private static final String WAA_RESPONSE_KEY = "history_recording_enabled";
    private static final String WAA_OAUTH_NAME = "web_history";

    private final Profile mProfile;

    /**
     * Creates a new instance.
     * @param profile the {@link Profile} to use when making the calls.
     */
    public CommerceSubscriptionsServiceProxy(Profile profile) {
        mProfile = profile;
    }

    /**
     * Makes an HTTPS call to the backend in order to create the provided subscriptions.
     * @param subscriptions list of {@link CommerceSubscription} to create.
     * @param callback indicates whether or not the operation succeeded on the backend.
     */
    public void create(List<CommerceSubscription> subscriptions, Callback<Boolean> callback) {
        if (subscriptions.isEmpty()) {
            callback.onResult(true);
            return;
        }

        manageSubscriptions(getCreateSubscriptionsRequestParams(subscriptions), callback);
    }

    /**
     * Makes an HTTPS call to the backend to delete the provided list of subscriptions.
     * @param subscriptions list of {@link CommerceSubscription} to delete.
     * @param callback indicates whether or not the operation succeeded on the backend.
     */
    public void delete(List<CommerceSubscription> subscriptions, Callback<Boolean> callback) {
        if (subscriptions.isEmpty()) {
            callback.onResult(true);
            return;
        }

        manageSubscriptions(getRemoveSubscriptionsRequestParams(subscriptions), callback);
    }

    /**
     * Fetches all subscriptions that match the provided type from the backend.
     * @param type the type of subscriptions to fetch.
     * @param callback contains the list of subscriptions returned from the server.
     */
    public void get(@CommerceSubscription.CommerceSubscriptionType String type,
            Callback<List<CommerceSubscription>> callback) {
        // TODO(crbug.com/995852): Replace MISSING_TRAFFIC_ANNOTATION with a real traffic
        // annotation.
        EndpointFetcher.fetchUsingOAuth(
                (response)
                        -> {
                    callback.onResult(createCommerceSubscriptions(response.getResponseString()));
                },
                mProfile, OAUTH_NAME,
                CommerceSubscriptionsServiceConfig.getDefaultServiceUrl()
                        + String.format(GET_SUBSCRIPTIONS_QUERY_PARAMS_TEMPLATE, type),
                GET_HTTPS_METHOD, CONTENT_TYPE, OAUTH_SCOPE, EMPTY_POST_DATA,
                HTTPS_REQUEST_TIMEOUT_MS, NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
    }

    void queryAndUpdateWaaEnabled() {
        // TODO(crbug.com/1311754): Move the endpoint fetch to components/ and merge this query to
        // shopping service. For NetworkTrafficAnnotationTag, we need to replace
        // MISSING_TRAFFIC_ANNOTATION with the correct NetworkTrafficAnnotation.
        EndpointFetcher.fetchUsingOAuth(
                (response)
                        -> {
                    try {
                        JSONObject object = new JSONObject(response.getResponseString());
                        boolean isWaaEnabled = object.getBoolean(WAA_RESPONSE_KEY);
                        PrefService prefService = UserPrefs.get(mProfile);
                        if (prefService != null) {
                            prefService.setBoolean(
                                    Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING, isWaaEnabled);
                        }
                    } catch (JSONException e) {
                        Log.e(TAG,
                                String.format(Locale.US, "Failed to get waa status. Details: %s",
                                        e.getMessage()));
                    }
                },
                mProfile, WAA_OAUTH_NAME, WAA_QUERY_URL, GET_HTTPS_METHOD, CONTENT_TYPE,
                WAA_OAUTH_SCOPE, EMPTY_POST_DATA, 30000L,
                NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
    }

    private void manageSubscriptions(JSONObject requestPayload, Callback<Boolean> callback) {
        // TODO(crbug.com/995852): Replace MISSING_TRAFFIC_ANNOTATION with a real traffic
        // annotation.
        EndpointFetcher.fetchUsingOAuth(
                (response)
                        -> {
                    callback.onResult(
                            didManageSubscriptionCallSucceed(response.getResponseString()));
                },
                mProfile, OAUTH_NAME, CommerceSubscriptionsServiceConfig.getDefaultServiceUrl(),
                POST_HTTPS_METHOD, CONTENT_TYPE, OAUTH_SCOPE, requestPayload.toString(),
                HTTPS_REQUEST_TIMEOUT_MS, NetworkTrafficAnnotationTag.MISSING_TRAFFIC_ANNOTATION);
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
