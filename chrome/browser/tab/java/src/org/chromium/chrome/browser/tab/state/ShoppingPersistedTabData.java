// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.ShoppingPersistedTabData.ShoppingPersistedTabDataProto;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping related websites
 */
public class ShoppingPersistedTabData extends PersistedTabData {
    private static final String TAG = "SPTD";
    private static final String ENDPOINT = "https://memex-pa.googleapis.com/v1/annotations?url=%s";
    private static final long TIMEOUT_MS = 1000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String OAUTH_NAME = "SPTD";
    private static final String EMPTY_POST_DATA = "";

    private static final String ANNOTATIONS_KEY = "annotations";
    private static final String PRICE_KEY = "price";
    private static final String TYPE_KEY = "type";
    private static final String BUYABLE_PRODUCT_ANNOTATION_KEY = "BUYABLE_PRODUCT";
    private static final String BUYABLE_PRODUCT_KEY = "buyableProduct";
    private static final String CURRENT_PRICE_KEY = "currentPrice";
    private static final String CURRENCY_CODE_KEY = "currencyCode";
    private static final String AMOUNT_MICROS_KEY = "amountMicros";
    private static final String ACCEPT_LANGUAGE_KEY = "Accept-Language";

    private static final int FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS = 2;
    private static final int FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS = 0;

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

    private String mCurrencyCode;

    @VisibleForTesting
    protected ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier =
            new ObservableSupplierImpl<>();

    @VisibleForTesting
    protected EmptyTabObserver mUrlUpdatedObserver;

    // Lazy initialization of OptimizationGuideBridgeFactory
    private static class OptimizationGuideBridgeFactoryHolder {
        private static final OptimizationGuideBridgeFactory sOptimizationGuideBridgeFactory;
        static {
            sOptimizationGuideBridgeFactory = new OptimizationGuideBridgeFactory(
                    Arrays.asList(HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR));
        }
    }

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

        @Override
        public boolean equals(Object object) {
            if (!(object instanceof PriceDrop)) return false;
            PriceDrop priceDrop = (PriceDrop) object;
            return this.price.equals(priceDrop.price)
                    && this.previousPrice.equals(priceDrop.previousPrice);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + (price == null ? 0 : price.hashCode());
            result = 31 * result + (previousPrice == null ? 0 : previousPrice.hashCode());
            return result;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public ShoppingPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(ShoppingPersistedTabData.class, tab.isIncognito())
                        .getId());
        setupPersistence(tab);
    }

    @VisibleForTesting
    protected ShoppingPersistedTabData(
            Tab tab, byte[] data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, storage, persistedTabDataId);
        deserializeAndLog(data);
        setupPersistence(tab);
    }

    private void setupPersistence(Tab tab) {
        // ShoppingPersistedTabData is not saved by default - only when its fields are populated
        // (after a successful endpoint repsonse)
        disableSaving();
        registerIsTabSaveEnabledSupplier(mIsTabSaveEnabledSupplier);
        mUrlUpdatedObserver = new EmptyTabObserver() {
            // When the URL is updated, the ShoppingPersistedTabData is redundant and should be
            // cleaned up.
            @Override
            public void onUrlUpdated(Tab tab) {
                disableSaving();
                if (tab.getUserDataHost().getUserData(ShoppingPersistedTabData.class) != null) {
                    tab.getUserDataHost().removeUserData(ShoppingPersistedTabData.class);
                }
            }
        };
        tab.addObserver(mUrlUpdatedObserver);
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
                    ShoppingPersistedTabData.isShoppingPage(tab.getUrl(), (isShoppingPage) -> {
                        if (!isShoppingPage) {
                            supplierCallback.onResult(null);
                            return;
                        }
                        EndpointFetcher.fetchUsingChromeAPIKey(
                                (endpointResponse)
                                        -> {
                                    supplierCallback.onResult(
                                            build(tab, endpointResponse.getResponseString(),
                                                    previousShoppingPersistedTabData));
                                },
                                Profile.getLastUsedRegularProfile(),
                                String.format(ENDPOINT, tab.getUrlString()), HTTPS_METHOD,
                                CONTENT_TYPE, EMPTY_POST_DATA, TIMEOUT_MS,
                                new String[] {ACCEPT_LANGUAGE_KEY,
                                        LocaleUtils.getDefaultLocaleListString()});
                    });
                },
                ShoppingPersistedTabData.class, callback);
    }

    /**
     * @return {@link ShoppingPersistedTabData} from {@link UserDataHost}
     */
    public static ShoppingPersistedTabData from(Tab tab) {
        return from(tab, USER_DATA_KEY);
    }

    /**
     * Whether a BuyableProductAnnotation was found or not
     */
    @IntDef({FoundBuyableProductAnnotation.NOT_FOUND, FoundBuyableProductAnnotation.FOUND})
    @Retention(RetentionPolicy.SOURCE)
    @interface FoundBuyableProductAnnotation {
        int NOT_FOUND = 0;
        int FOUND = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Enable saving of {@link ShoppingPersistedTabData}
     */
    protected void enableSaving() {
        mIsTabSaveEnabledSupplier.set(true);
    }

    /**
     * Disable saving of {@link ShoppingPersistedTabData}. Deletes previously saved {@link
     * ShoppingPersistedTabData} as well.
     */
    public void disableSaving() {
        mIsTabSaveEnabledSupplier.set(false);
    }

    private static ShoppingPersistedTabData build(Tab tab, String responseString,
            ShoppingPersistedTabData previousShoppingPersistedTabData) {
        ShoppingPersistedTabData res = new ShoppingPersistedTabData(tab);
        @FoundBuyableProductAnnotation
        int foundBuyableProductAnnotation = FoundBuyableProductAnnotation.NOT_FOUND;
        try {
            JSONObject jsonObject = new JSONObject(responseString);
            JSONArray annotations = jsonObject.getJSONArray(ANNOTATIONS_KEY);
            for (int i = 0; i < annotations.length(); i++) {
                JSONObject annotation = annotations.getJSONObject(i);
                if (BUYABLE_PRODUCT_ANNOTATION_KEY.equals(annotation.getString(TYPE_KEY))) {
                    JSONObject metadata = annotation.getJSONObject(BUYABLE_PRODUCT_KEY);
                    JSONObject priceMetadata = metadata.getJSONObject(CURRENT_PRICE_KEY);
                    res.setPriceMicros(Long.parseLong(priceMetadata.getString(AMOUNT_MICROS_KEY)),
                            previousShoppingPersistedTabData);
                    res.setCurrencyCode(priceMetadata.getString(CURRENCY_CODE_KEY));
                    res.setLastUpdatedMs(System.currentTimeMillis());
                    foundBuyableProductAnnotation = FoundBuyableProductAnnotation.FOUND;
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
        RecordHistogram.recordEnumeratedHistogram(
                "Tabs.ShoppingPersistedTabData.FoundBuyableProductAnnotation",
                foundBuyableProductAnnotation, FoundBuyableProductAnnotation.NUM_ENTRIES);
        // Only persist this ShoppingPersistedTabData if it was correctly populated from the
        // response
        if (foundBuyableProductAnnotation == FoundBuyableProductAnnotation.FOUND) {
            res.enableSaving();
            return res;
        }
        return null;
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

    protected void setCurrencyCode(String currencyCode) {
        mCurrencyCode = currencyCode;
        save();
    }

    @VisibleForTesting
    protected String getCurrencyCode() {
        return mCurrencyCode;
    }

    @VisibleForTesting
    protected void setPreviousPriceMicros(long previousPriceMicros) {
        mPreviousPriceMicros = previousPriceMicros;
        save();
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
    private String formatPrice(long priceMicros) {
        CurrencyFormatter currencyFormatter =
                new CurrencyFormatter(mCurrencyCode, Locale.getDefault());
        String formattedPrice;
        if (priceMicros < TEN_UNITS) {
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_LESS_THAN_TEN_UNITS);
            formattedPrice = String.format(
                    Locale.getDefault(), "%.2f", (100 * priceMicros / MICROS_TO_UNITS) / 100.0);
        } else {
            currencyFormatter.setMaximumFractionalDigits(FRACTIONAL_DIGITS_GREATER_THAN_TEN_UNITS);
            formattedPrice = String.format(Locale.getDefault(), "%d",
                    (long) Math.floor(
                            (double) (priceMicros + MICROS_TO_UNITS / 2) / MICROS_TO_UNITS));
        }
        return currencyFormatter.format(formattedPrice);
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
        // Do not attempt to deserialize if the bytes are null
        if (bytes == null) {
            return false;
        }
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

    @Override
    public void destroy() {
        mTab.removeObserver(mUrlUpdatedObserver);
        super.destroy();
    }

    private static void isShoppingPage(GURL url, Callback<Boolean> callback) {
        OptimizationGuideBridgeFactoryHolder.sOptimizationGuideBridgeFactory.create()
                .canApplyOptimization(url, HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR,
                        (decision, metadata) -> {
                            callback.onResult(decision == OptimizationGuideDecision.TRUE
                                    || decision == OptimizationGuideDecision.UNKNOWN);
                        });
    }
}