// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.ShoppingPersistedTabData.ShoppingPersistedTabDataProto;

import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping related websites
 */
public class ShoppingPersistedTabData extends PersistedTabData {
    private static final String TAG = "SPTD";
    private static final String ENDPOINT =
            "https://task-management-chrome.sandbox.google.com/tabs/representations?url=%s&locale=en:US";
    private static final String[] SCOPES =
            new String[] {"https://www.googleapis.com/auth/userinfo.email",
                    "https://www.googleapis.com/auth/userinfo.profile"};
    private static final long TIMEOUT_MS = 1000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String OAUTH_NAME = "SPTD";
    private static final String EMPTY_POST_DATA = "";

    private static final String REPRESENTATIONS_KEY = "representations";
    private static final String PRICE_KEY = "price";
    private static final String TYPE_KEY = "type";
    private static final String SHOPPING_ID = "SHOPPING";

    private static final Class<ShoppingPersistedTabData> USER_DATA_KEY =
            ShoppingPersistedTabData.class;
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final long NO_TRANSITIONS_OCCURRED = -1;

    private long mTimeToLiveMs = ONE_HOUR_MS;
    private String mPriceString = "";
    private String mPreviousPriceString = "";
    public long mLastPriceChangeTimeMs = NO_TRANSITIONS_OCCURRED;

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ShoppingPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .storage,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .id);
    }

    private ShoppingPersistedTabData(
            Tab tab, byte[] data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, data, storage, persistedTabDataId);
    }

    /**
     * Acquire {@link ShoppingPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} ShoppingPersistedTabData is acquired for
     * @param callback {@link Callback} {@link ShoppingPersistedTabData is passed back in}
     */
    public static void from(Tab tab, Callback<ShoppingPersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new ShoppingPersistedTabData(tab, data, storage, id); },
                new OneshotSupplierImpl<ShoppingPersistedTabData>() {
                    @Override
                    public void onAvailable(Callback<ShoppingPersistedTabData> supplierCallback) {
                        ShoppingPersistedTabData previousShoppingPersistedTabData =
                                PersistedTabData.from(tab, USER_DATA_KEY);
                        EndpointFetcher.fetchUsingOAuth(
                                (endpointResponse)
                                        -> {
                                    supplierCallback.onResult(
                                            build(tab, endpointResponse.getResponseString(),
                                                    previousShoppingPersistedTabData));
                                },
                                Profile.getLastUsedRegularProfile(), OAUTH_NAME,
                                String.format(Locale.US, ENDPOINT, tab.getUrlString()),
                                HTTPS_METHOD, CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS);
                    }
                },
                ShoppingPersistedTabData.class, callback);
    }

    private static ShoppingPersistedTabData build(Tab tab, String responseString,
            ShoppingPersistedTabData previousShoppingPersistedTabData) {
        ShoppingPersistedTabData res = new ShoppingPersistedTabData(tab);
        try {
            JSONObject jsonObject = new JSONObject(responseString);
            JSONArray representations = jsonObject.getJSONArray(REPRESENTATIONS_KEY);
            for (int i = 0; i < representations.length(); i++) {
                JSONObject representation = representations.getJSONObject(i);
                // TODO(crbug.com/1130068) support all currencies
                if (SHOPPING_ID.equals(representation.getString(TYPE_KEY))) {
                    res.setPriceString(
                            String.format(Locale.US, "$%.2f", representation.getDouble(PRICE_KEY)),
                            previousShoppingPersistedTabData);
                    break;
                }
            }
        } catch (JSONException e) {
            Log.i(TAG,
                    String.format(Locale.US,
                            "There was a problem acquiring "
                                    + "ShoppingPersistedTabData "
                                    + "Details: %s",
                            e.toString()));
        }
        return res;
    }

    /**
     * Set the price string
     * @param priceString a string representing the price of the shopping offer
     * @param previousShoppingPersistedTabData {@link ShoppingPersistedTabData} from previous fetch
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void setPriceString(
            String priceString, ShoppingPersistedTabData previousShoppingPersistedTabData) {
        mPriceString = priceString;
        // Detect price transition
        if (previousShoppingPersistedTabData != null && !TextUtils.isEmpty(priceString)
                && !TextUtils.isEmpty(previousShoppingPersistedTabData.getPriceString())
                && !priceString.equals(previousShoppingPersistedTabData.getPriceString())) {
            mPreviousPriceString = previousShoppingPersistedTabData.getPriceString();
            mLastPriceChangeTimeMs = SystemClock.uptimeMillis();
        } else if (previousShoppingPersistedTabData != null) {
            mPreviousPriceString = previousShoppingPersistedTabData.getPreviousPriceString();
            mLastPriceChangeTimeMs = previousShoppingPersistedTabData.getLastPriceChangeTimeMs();
        }
        save();
    }

    /**
     * @return a price representing the price of the shopping offer
     * TODO(crbug.com/1130067) add timeouts to {@link ShoppingPersistedTabData}
     * properties so they stay in sync with the actual price.
     * Or should we have a push model?
     */
    public String getPriceString() {
        return mPriceString;
    }

    /**
     * @return the price string of the {@link ShoppingPersistedTabDta}
     * before the refetch occurred because of a timeout. This enables
     * the consumer to determine if the price changed during the fetch.
     */
    public String getPreviousPriceString() {
        return mPreviousPriceString;
    }

    @Override
    public byte[] serialize() {
        return ShoppingPersistedTabDataProto.newBuilder()
                .setPriceString(mPriceString)
                .setPreviousPriceString(mPreviousPriceString)
                .setLastUpdatedMs(getLastUpdatedMs())
                .setLastPriceChangeTimeMs(mLastPriceChangeTimeMs)
                .build()
                .toByteArray();
    }

    @Override
    public boolean deserialize(@Nullable byte[] bytes) {
        // TODO(crbug.com/1135573) add in metrics for serialize and deserialize
        try {
            ShoppingPersistedTabDataProto shoppingPersistedTabDataProto =
                    ShoppingPersistedTabDataProto.parseFrom(bytes);
            mPriceString = shoppingPersistedTabDataProto.getPriceString();
            mPreviousPriceString = shoppingPersistedTabDataProto.getPreviousPriceString();
            setLastUpdatedMs(shoppingPersistedTabDataProto.getLastUpdatedMs());
            mLastPriceChangeTimeMs = shoppingPersistedTabDataProto.getLastPriceChangeTimeMs();
            return true;
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "There was a problem deserializing "
                                    + "ShoppingPersistedTabData. Details: %s",
                            e.getMessage()));
        }
        return false;
    }

    @Override
    public void destroy() {}

    @Override
    public String getUmaTag() {
        return "SPTD";
    }

    @Override
    public long getTimeToLiveMs() {
        return mTimeToLiveMs;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void setTimeToLiveMs(long timeToLiveMs) {
        mTimeToLiveMs = timeToLiveMs;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public long getLastPriceChangeTimeMs() {
        return mLastPriceChangeTimeMs;
    }
}