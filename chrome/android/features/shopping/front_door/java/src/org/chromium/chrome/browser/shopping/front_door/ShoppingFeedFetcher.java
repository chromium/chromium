// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.shopping.front_door;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * This is used to fetch the front door feed endpoint.
 */
public class ShoppingFeedFetcher {
    public interface CountryCodeProvider {
        public String getCountryCode();
    }
    // Shared param
    private static final String GET_METHOD = "GET";
    private static final String POST_METHOD = "POST";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final long TIMEOUT_MS = 10000L;
    private static final String EMPTY_POST_DATA = "";

    // Offer item request param
    private static final String ENDPOINT = "https://memex-pa.googleapis.com/v1/shopping/feed";
    private static final String[] OAUTH_SCOPE =
            new String[] {"https://www.googleapis.com/auth/chromememex"};
    private static final String OAUTH_NAME = "shopping_feed";

    // Product line item request param
    private static final String PRODUCT_LINE_ENDPOINT =
            "https://memex-pa.googleapis.com/v1/shopping/brands";

    // Brand item request param
    private static final String CATEGORY_TO_BRANDS_ENDPOINT =
            "https://memex-pa.googleapis.com/v1/shopping/category/brands";

    // TODO(meiliang): Callback takes in a list of Feed modules.
    public static void fetch(Callback<String> callback) {
        Log.i("FrontDoor", "Client using EndpointFetcher to fetch");
        EndpointFetcher.fetchUsingOAuth(
                (endpointResponse)
                        -> {
                    // TODO(meiliang): Build modules from endpointResponse.
                    callback.onResult(endpointResponse.getResponseString());
                },
                Profile.getLastUsedRegularProfile(), OAUTH_NAME, ENDPOINT, GET_METHOD, CONTENT_TYPE,
                OAUTH_SCOPE, EMPTY_POST_DATA, TIMEOUT_MS, true);

        Log.e("Meil_ShoppingFeedFetcher", "fetch offer item: ");
        Log.e("Meil_ShoppingFeedFetcher", "endpoint: " + ENDPOINT);
        Log.e("Meil_ShoppingFeedFetcher", "method: " + GET_METHOD);
        Log.e("Meil_ShoppingFeedFetcher", "OAuth name: " + OAUTH_NAME);
        Log.e("Meil_ShoppingFeedFetcher", "OAuth scope: " + Arrays.toString(OAUTH_SCOPE));
        Log.e("Meil_ShoppingFeedFetcher", "content type: " + CONTENT_TYPE);
        Log.e("Meil_ShoppingFeedFetcher", "request body: " + EMPTY_POST_DATA);
        Log.e("Meil_ShoppingFeedFetcher", "time out: " + TIMEOUT_MS);
    }

    // Fetch Product Lines
    /**
     * Fetch product line datas for the given list of brand ids. The list of brand ids can be empty.
     * If it's empty, the end point should return product lines from a default list.
     * @param brandIds List of interested brand ids. This can be an empty list.
     * @param callback Function to call when response comes back.
     */
    public static void fetchProductLine(List<String> brandIds, Callback<String> callback) {
        String[] headers =
                new String[] {"Accept-Language", LocaleUtils.getDefaultLocaleListString()};
        EndpointFetcher.fetchUsingChromeAPIKey(
                (endpoindResponse)
                        -> { callback.onResult(endpoindResponse.getResponseString()); },
                Profile.getLastUsedRegularProfile(), PRODUCT_LINE_ENDPOINT, POST_METHOD,
                CONTENT_TYPE, buildPostBody(brandIds, "brandMids"), TIMEOUT_MS, headers);

        Log.e("Meil_ShoppingFeedFetcher", "fetch product line: ");
        Log.e("Meil_ShoppingFeedFetcher", "endpoint: " + PRODUCT_LINE_ENDPOINT);
        Log.e("Meil_ShoppingFeedFetcher", "method: " + POST_METHOD);
        Log.e("Meil_ShoppingFeedFetcher", "content type: " + CONTENT_TYPE);
        Log.e("Meil_ShoppingFeedFetcher", "headers: " + Arrays.toString(headers));
        Log.e("Meil_ShoppingFeedFetcher", "request body: " + buildPostBody(brandIds, "brandMids"));
        Log.e("Meil_ShoppingFeedFetcher", "time out: " + TIMEOUT_MS);
    }

    private static String buildPostBody(List<String> ids, String keyString) {
        JSONObject object = new JSONObject();
        try {
            object.put(keyString, new JSONArray(ids));
        } catch (JSONException e) {
            Log.e("Meil",
                    "building post body json for fetching product lines error: " + e.getMessage());
            e.printStackTrace();
        }
        return object.toString();
    }

    // Fetch brands for Onboarding.
    public static void fetchBrandsForCategories(List<String> categoryKeys,
            Callback<String> callback, CountryCodeProvider countryCodeProvider) {
        JSONObject jsonRequestObject = new JSONObject();
        try {
            jsonRequestObject.put("categoryIds", new JSONArray(categoryKeys));
            String country = ContextUtils.getApplicationContext()
                                     .getResources()
                                     .getConfiguration()
                                     .locale.getCountry()
                                     .toLowerCase();
            Log.e("Meil_ShoppingFeedFetcher", "Country code: " + country);
            country = !country.equals(Locale.JAPAN.getCountry().toLowerCase()) ? "us" : country;
            jsonRequestObject.put("studyRegion", country);
        } catch (JSONException e) {
            Log.e("Meil",
                    "building post body json for fetching product lines error: " + e.getMessage());
            e.printStackTrace();
        }

        EndpointFetcher.fetchUsingChromeAPIKey(
                (endpoindResponse)
                        -> { callback.onResult(endpoindResponse.getResponseString()); },
                Profile.getLastUsedRegularProfile(), CATEGORY_TO_BRANDS_ENDPOINT, POST_METHOD,
                CONTENT_TYPE, jsonRequestObject.toString(), TIMEOUT_MS, new String[1]);

        Log.e("Meil_ShoppingFeedFetcher", "fetch brands for categories: ");
        Log.e("Meil_ShoppingFeedFetcher", "endpoint: " + CATEGORY_TO_BRANDS_ENDPOINT);
        Log.e("Meil_ShoppingFeedFetcher", "method: " + POST_METHOD);
        Log.e("Meil_ShoppingFeedFetcher", "content type: " + CONTENT_TYPE);
        Log.e("Meil_ShoppingFeedFetcher", "request body: " + jsonRequestObject.toString());
        Log.e("Meil_ShoppingFeedFetcher", "time out: " + TIMEOUT_MS);
    }
}
