// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;


import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
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
    @VisibleForTesting
    public static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);
    private static final int MICROS_TO_UNITS = 1000000;
    private static final long TWO_UNITS = 2 * MICROS_TO_UNITS;
    private static final long TEN_UNITS = 10 * MICROS_TO_UNITS;
    private static final int MINIMUM_DROP_PERCENTAGE = 10;
    private static final long ONE_WEEK_MS = TimeUnit.DAYS.toMillis(7);

    @VisibleForTesting
    public static final long NO_TRANSITIONS_OCCURRED = -1;

    @VisibleForTesting
    public static final long NO_PRICE_KNOWN = -1;

    private long mTimeToLiveMs = ONE_HOUR_MS;
    public long mLastPriceChangeTimeMs = NO_TRANSITIONS_OCCURRED;

    private long mPriceMicros = NO_PRICE_KNOWN;
    private long mPreviousPriceMicros = NO_PRICE_KNOWN;

    /**
     * A price drop for the offer {@link ShoppingPersistedTabData}
     * refers to
     */
    public static class PriceDrop {
        public final String price;
        public final String previousPrice;

        /**
         * @param price {@link String} representation of the price
         * @param previousPrice {@link String} representation of the previous price
         */
        public PriceDrop(String price, String previousPrice) {
            this.price = price;
            this.previousPrice = previousPrice;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ShoppingPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getId());
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
                (supplierCallback)
                        -> {
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
                            String.format(Locale.US, ENDPOINT, tab.getUrlString()), HTTPS_METHOD,
                            CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS);
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
                if (SHOPPING_ID.equals(representation.getString(TYPE_KEY))) {
                    res.setPriceMicros(
                            representation.getLong(PRICE_KEY), previousShoppingPersistedTabData);
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
    public void setPriceMicros(
            long priceMicros, ShoppingPersistedTabData previousShoppingPersistedTabData) {
        mPriceMicros = priceMicros;
        // Detect price transition
        if (previousShoppingPersistedTabData != null && priceMicros != NO_PRICE_KNOWN
                && previousShoppingPersistedTabData.getPriceMicros() != NO_PRICE_KNOWN
                && priceMicros != previousShoppingPersistedTabData.getPriceMicros()) {
            mPreviousPriceMicros = previousShoppingPersistedTabData.getPriceMicros();
            mLastPriceChangeTimeMs = System.currentTimeMillis();
        } else if (previousShoppingPersistedTabData != null) {
            mPreviousPriceMicros = previousShoppingPersistedTabData.getPreviousPriceMicros();
            mLastPriceChangeTimeMs = previousShoppingPersistedTabData.getLastPriceChangeTimeMs();
        }
        save();
    }

    @VisibleForTesting
    protected void setPreviousPriceMicros(long previousPriceMicros) {
        mPreviousPriceMicros = previousPriceMicros;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPriceMicros() {
        return mPriceMicros;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public long getPreviousPriceMicros() {
        return mPreviousPriceMicros;
    }

    @VisibleForTesting
    public void setPriceMicrosForTesting(long priceMicros) {
        mPriceMicros = priceMicros;
    }

    @VisibleForTesting
    public void setPreviousPriceMicrosForTesting(long previousPriceMicros) {
        mPreviousPriceMicros = previousPriceMicros;
    }

    /**
     * @return {@link PriceDrop} relating to the offer for the {@link ShoppingPersistedTabData}
     * TODO(crbug.com/1145770) Implement getPriceDrop to only return a result if there is
     * actually a price drop. Ensure priceString and previousPriceString are integers.
     * Deprecate getPrice and getPriceString(). Change price and previousPriceString
     * representations to be numeric to make drop comparison easier.
     */
    public PriceDrop getPriceDrop() {
        if (mPriceMicros == NO_PRICE_KNOWN || mPreviousPriceMicros == NO_PRICE_KNOWN
                || !isQualifyingPriceDrop() || isPriceChangeStale()) {
            return null;
        }
        String formattedPrice = formatPrice(mPriceMicros);
        String formattedPreviousPrice = formatPrice(mPreviousPriceMicros);
        if (formattedPrice.equals(formattedPreviousPrice)) {
            return null;
        }
        return new PriceDrop(formattedPrice, formattedPreviousPrice);
    }

    private boolean isPriceChangeStale() {
        return mLastPriceChangeTimeMs != NO_TRANSITIONS_OCCURRED
                && System.currentTimeMillis() - mLastPriceChangeTimeMs > ONE_WEEK_MS;
    }

    private boolean isQualifyingPriceDrop() {
        if (mPreviousPriceMicros - mPriceMicros < getMinimumDropThresholdAbsolute()) {
            return false;
        }
        if ((100L * mPriceMicros) / mPreviousPriceMicros
                > (100 - getMinimumDroppedThresholdPercentage())) {
            return false;
        }
        return true;
    }

    // TODO(crbug.com/1151156) Make parameters finch configurable
    private static int getMinimumDroppedThresholdPercentage() {
        return MINIMUM_DROP_PERCENTAGE;
    }

    private static long getMinimumDropThresholdAbsolute() {
        return TWO_UNITS;
    }

    // TODO(crbug.com/1130068) support all currencies
    private static String formatPrice(long priceMicros) {
        if (priceMicros < TEN_UNITS) {
            return String.format(Locale.US, "$%.2f", (100 * priceMicros / MICROS_TO_UNITS) / 100.0);
        }
        return String.format(Locale.US, "$%d",
                (int) Math.floor((double) (priceMicros + MICROS_TO_UNITS / 2) / MICROS_TO_UNITS));
    }

    @Override
    public byte[] serialize() {
        return ShoppingPersistedTabDataProto.newBuilder()
                .setPriceMicros(mPriceMicros)
                .setPreviousPriceMicros(mPreviousPriceMicros)
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
            mPriceMicros = shoppingPersistedTabDataProto.getPriceMicros();
            mPreviousPriceMicros = shoppingPersistedTabDataProto.getPreviousPriceMicros();
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

    @VisibleForTesting
    public void setTimeToLiveMs(long timeToLiveMs) {
        mTimeToLiveMs = timeToLiveMs;
    }

    @VisibleForTesting
    public long getLastPriceChangeTimeMs() {
        return mLastPriceChangeTimeMs;
    }

    @VisibleForTesting
    public void setLastPriceChangeTimeMsForTesting(long lastPriceChangeTimeMs) {
        mLastPriceChangeTimeMs = lastPriceChangeTimeMs;
    }
}