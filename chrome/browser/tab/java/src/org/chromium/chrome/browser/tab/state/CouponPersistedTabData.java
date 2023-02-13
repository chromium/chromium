// Copyright 2022 The Chromium Authors
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
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.commerce.PriceUtils;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.CouponPersistedTabData.CouponPersistedTabDataProto;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping websites with coupons.
 *
 */
public class CouponPersistedTabData extends PersistedTabData {
    private static final String ENDPOINT =
            "https://task-management-chrome.sandbox.google.com/promotions?url=%s";
    private static final String[] SCOPES =
            new String[] {"https://www.googleapis.com/auth/userinfo.email",
                    "https://www.googleapis.com/auth/userinfo.profile"};
    private static final long TIMEOUT_MS = 10000L;
    private static final String HTTPS_METHOD = "GET";
    private static final String CONTENT_TYPE = "application/json; charset=UTF-8";
    private static final String EMPTY_POST_DATA = "";
    private static final String PERSISTED_TAB_DATA_ID = "COPTD";
    private static final String TAG = "COPTD";
    private static final Class<CouponPersistedTabData> USER_DATA_KEY = CouponPersistedTabData.class;
    private static final String DISCOUNTS_ARRAY = "discounts";
    private static final String DISCOUNT_INFO_OBJECT = "freeListingDiscountInfo";
    private static final String LONG_TITLE_STRING = "longTitle";
    private static final String COUPON_CODE_STRING = "couponCode";
    private static final long ONE_HOUR_MS = TimeUnit.HOURS.toMillis(1);
    private static final String AMOUNT_OFF_OBJECT = "amountOff";
    private static final String PERCENT_OFF_STRING = "percentOff";
    private static final String UNITS_STRING = "units";
    private static final String CURRENCY_CODE_STRING = "currencyCode";
    private static final int MICROS_TO_UNITS = 1000000;
    private static final String DEFAULT_ANNOTATION_TEXT = "Coupon Available";
    private static final String TYPE_PERCENTAGE = "%";
    // TODO(crbug.com/1347575) Make coupon annotation text localizable.
    private static final String OFF_STRING = " Off";
    private CurrencyFormatter mCurrencyFormatter;

    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete("coupon_persisted_tab_data",
                    "semantics {"
                            + "  sender: 'Coupon Data Acquisition (Android)'"
                            + "  description: 'Acquires discount data for a given URL '"
                            + "  trigger: 'When the URL for a Tab changes'"
                            + "  data: 'URL the user is navigating to (in a non-incognito Tab).'"
                            + "  destination: OTHER"
                            + "}"
                            + "policy {"
                            + "  cookies_allowed: NO"
                            + "  setting: 'This feature cannot be disabled by settings although '"
                            + "           'is gated by a flag'"
                            + "  policy_exception_justification:"
                            + "      'Not implemented.'"
                            + "}");

    @VisibleForTesting
    protected EmptyTabObserver mUrlUpdatedObserver;
    @VisibleForTesting
    protected ObservableSupplierImpl<Boolean> mIsTabSaveEnabledSupplier =
            new ObservableSupplierImpl<>();
    private Coupon mCoupon;

    /**
     * Acquire {@link CouponPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} CouponPersistedTabData is created for
     * @param coupon {@link Coupon} for the website open in this tab
     */
    CouponPersistedTabData(Tab tab, Coupon coupon) {
        this(tab);
        mCoupon = coupon;
    }

    /**
     * Acquire {@link CouponPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} CouponPersistedTabData is created for
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public CouponPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(CouponPersistedTabData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(CouponPersistedTabData.class, tab.isIncognito())
                        .getId());
        setupPersistence(tab);
    }

    /**
     * Coupon data type for {@link CouponPersistedTabData}
     */
    public static class Coupon {
        public String couponName;
        public String promoCode;
        public String currencyCode;
        public long discountUnits;
        public DiscountType discountType;

        @VisibleForTesting
        public enum DiscountType {
            PERCENT_OFF,
            AMOUNT_OFF,
            UNKNOWN,
        }

        public Coupon() {
            couponName = null;
            promoCode = null;
            discountType = null;
            discountUnits = -1;
        }

        /**
         * @param promo promotional code relating to coupon
         * @param currency currency code for coupons that offer fixed amount off, otherwise null
         * @param name name of coupon
         * @param units numeric monetary amount or percentage being offered in coupon
         * @param type type of discount (percent or amount off)
         */
        public Coupon(String name, String promo, String currency, long units, DiscountType type) {
            couponName = name;
            promoCode = promo;
            currencyCode = currency;
            discountUnits = units;
            discountType = type;
        }

        public boolean hasCoupon() {
            return couponName != null && promoCode != null;
        }
    }

    private CouponPersistedTabDataProto.DiscountType getDiscountType(Coupon.DiscountType type) {
        switch (type) {
            case AMOUNT_OFF:
                return CouponPersistedTabDataProto.DiscountType.AMOUNT_OFF;
            case PERCENT_OFF:
                return CouponPersistedTabDataProto.DiscountType.PERCENT_OFF;
            default:
                assert false : "Unexpected serialization of DiscountType: " + type;
                return CouponPersistedTabDataProto.DiscountType.UNKNOWN;
        }
    }

    private Coupon.DiscountType getDiscountType(CouponPersistedTabDataProto.DiscountType type) {
        switch (type) {
            case AMOUNT_OFF:
                return Coupon.DiscountType.AMOUNT_OFF;
            case PERCENT_OFF:
                return Coupon.DiscountType.PERCENT_OFF;
            default:
                assert false : "Unexpected deserialization of DiscountType: " + type;
                return Coupon.DiscountType.UNKNOWN;
        }
    }

    /**
     * @return {@link Coupon} relating to the main offer in the page, if available
     */
    @VisibleForTesting
    protected Coupon getCoupon() {
        if (mCoupon == null || !mCoupon.hasCoupon()) {
            return null;
        }
        return mCoupon;
    }

    /**
     * @return {@link String} composed from data stored in the {@link Coupon} relating to
     * the main offer in the page, if available.
     */
    public String getCouponAnnotationText() {
        if (getCoupon() == null) {
            return null;
        }
        try {
            if (getCoupon().discountType == Coupon.DiscountType.PERCENT_OFF) {
                return getCoupon().discountUnits + TYPE_PERCENTAGE + OFF_STRING;
            } else if (getCoupon().discountType == Coupon.DiscountType.AMOUNT_OFF
                    && mCoupon.currencyCode != null && !mCoupon.currencyCode.isEmpty()) {
                // TODO(crbug.com/1346404): Cache currency formatters.
                // CurrencyFormatter can throw exception when given malformed currency code.
                mCurrencyFormatter =
                        new CurrencyFormatter(mCoupon.currencyCode, Locale.getDefault());
                long unitsInMicros = mCoupon.discountUnits * MICROS_TO_UNITS;

                String annotationText =
                        PriceUtils.formatPrice(mCurrencyFormatter, unitsInMicros) + OFF_STRING;
                mCurrencyFormatter.destroy();

                return annotationText;
            }
            return DEFAULT_ANNOTATION_TEXT;
        } catch (Exception e) {
            Log.e(TAG,
                    "Returning default annotation text, "
                            + "error obtaining coupon annotation text: %s",
                    e.getMessage());
            return DEFAULT_ANNOTATION_TEXT;
        }
    }

    @Override
    Serializer<ByteBuffer> getSerializer() {
        CouponPersistedTabDataProto.Builder builder = CouponPersistedTabDataProto.newBuilder();
        if (mCoupon != null) {
            if (mCoupon.promoCode != null) {
                builder.setCode(mCoupon.promoCode);
            }

            if (mCoupon.couponName != null) {
                builder.setName(mCoupon.couponName);
            }

            if (mCoupon.currencyCode != null) {
                builder.setCurrencyCode(mCoupon.currencyCode);
            }

            if (mCoupon.discountType != null) {
                builder.setDiscountType(getDiscountType(mCoupon.discountType));
            }
            builder.setDiscountUnits(mCoupon.discountUnits);
        }
        return () -> {
            return builder.build().toByteString().asReadOnlyByteBuffer();
        };
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        // Do not attempt to deserialize if the bytes are null
        if (bytes == null || !bytes.hasRemaining()) {
            return false;
        }
        try {
            CouponPersistedTabDataProto couponPersistedTabDataProto =
                    CouponPersistedTabDataProto.parseFrom(bytes);
            mCoupon = new Coupon(couponPersistedTabDataProto.getName(),
                    couponPersistedTabDataProto.getCode(),
                    couponPersistedTabDataProto.getCurrencyCode(),
                    couponPersistedTabDataProto.getDiscountUnits(),
                    getDiscountType(couponPersistedTabDataProto.getDiscountType()));
            return true;
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG,
                    String.format(Locale.US,
                            "There was a problem deserializing "
                                    + "CouponPersistedTabData. Details: %s",
                            e.getMessage()));
        }
        return false;
    }

    @Override
    public String getUmaTag() {
        return TAG;
    }

    /**
     * @return {@link CouponPersistedTabData} from {@link UserDataHost}
     */
    public static CouponPersistedTabData from(Tab tab) {
        CouponPersistedTabData couponPersistedTabData = PersistedTabData.from(tab, USER_DATA_KEY);
        if (couponPersistedTabData == null) {
            couponPersistedTabData = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new CouponPersistedTabData(tab));
        }
        return couponPersistedTabData;
    }

    /**
     * Acquire {@link CouponPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} that {@link CouponPersistedTabData} is acquired for
     * @param callback {@link Callback} that {@link CouponPersistedTabData} is passed back in
     */
    public static void from(Tab tab, Callback<CouponPersistedTabData> callback) {
        if (tab == null || tab.isDestroyed() || tab.isIncognito() || tab.isCustomTab()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { callback.onResult(null); });
            return;
        }
        PersistedTabData.from(tab,
                (data, storage, id, factoryCallback)
                        -> {
                    CouponPersistedTabData couponPersistedTabData =
                            tab.isDestroyed() ? null : CouponPersistedTabData.from(tab);
                    if (couponPersistedTabData != null) {
                        couponPersistedTabData.deserializeAndLog(data);
                    }
                    factoryCallback.onResult(couponPersistedTabData);
                },
                (supplierCallback)
                        -> {
                    if (tab.isDestroyed()) {
                        supplierCallback.onResult(null);
                        return;
                    }
                    EndpointFetcher.fetchUsingOAuth(
                            (endpointResponse)
                                    -> {
                                supplierCallback.onResult(
                                        build(tab, endpointResponse.getResponseString()));
                            },
                            Profile.getLastUsedRegularProfile(), PERSISTED_TAB_DATA_ID,
                            String.format(Locale.US, ENDPOINT, tab.getUrl().getSpec()),
                            HTTPS_METHOD, CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS,
                            TRAFFIC_ANNOTATION);
                },
                CouponPersistedTabData.class, callback);
    }

    @Nullable
    private static CouponPersistedTabData build(Tab tab, String responseString) {
        Coupon coupon;
        coupon = getCouponFromJSON(responseString);
        if (coupon == null || !coupon.hasCoupon()) {
            return null;
        }
        CouponPersistedTabData res = new CouponPersistedTabData(tab, coupon);
        res.enableSaving();
        return res;
    }

    private static Coupon getCouponFromJSON(String jsonString) {
        String name;
        String promoCode;
        String currencyCode;
        Coupon.DiscountType type;
        long units;
        try {
            JSONObject jsonObject = new JSONObject(jsonString);
            JSONArray discountsArray = jsonObject.optJSONArray(DISCOUNTS_ARRAY);
            JSONObject discountInfo =
                    discountsArray.optJSONObject(0).optJSONObject(DISCOUNT_INFO_OBJECT);
            name = discountInfo.optString(LONG_TITLE_STRING);
            promoCode = discountInfo.optString(COUPON_CODE_STRING);

            JSONObject amountOff = discountsArray.optJSONObject(0).optJSONObject(AMOUNT_OFF_OBJECT);

            if (amountOff == null) {
                type = Coupon.DiscountType.PERCENT_OFF;
                currencyCode = TYPE_PERCENTAGE;
                units = Long.parseLong(
                        discountsArray.optJSONObject(0).optString(PERCENT_OFF_STRING));
            } else {
                type = Coupon.DiscountType.AMOUNT_OFF;
                currencyCode = amountOff.optString(CURRENCY_CODE_STRING);
                units = Long.parseLong(amountOff.optString(UNITS_STRING));
            }
        } catch (JSONException e) {
            Log.e(TAG, "Error converting response to JSON: %s", e.getMessage());
            return null;
        } catch (NullPointerException e) {
            // Can occur if value mapped by name does not exist when using optJSONObject because
            // null object will be returned.
            Log.e(TAG,
                    "NullPointerException occurred while"
                            + " building coupon from JSON: %s",
                    e.getMessage());
            return null;
        } catch (Exception e) {
            Log.i(TAG, "Exception occurred while building COPTD: %s", e.getMessage());
            return null;
        }
        return new Coupon(name, promoCode, currencyCode, units, type);
    }

    private void setupPersistence(Tab tab) {
        // CouponPersistedTabData is not saved by default - only when its fields are populated
        // (after a successful endpoint response)
        disableSaving();
        registerIsTabSaveEnabledSupplier(mIsTabSaveEnabledSupplier);
        mUrlUpdatedObserver = new EmptyTabObserver() {
            @Override
            public void onDidStartNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigationHandle) {
                if (navigationHandle.isSameDocument()) {
                    return;
                }
                // User is navigating to a different page - as detected by a change in URL
                if (!tab.getUrl().equals(navigationHandle.getUrl())) {
                    resetCoupon();
                }
            }

            @Override
            public void onDidFinishNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigationHandle) {
                String scheme = navigationHandle.getUrl().getScheme();
                if (!scheme.equals(UrlConstants.HTTP_SCHEME)
                        && !scheme.equals(UrlConstants.HTTPS_SCHEME)) {
                    return;
                }
                resetCoupon();
                prefetchOnNewNavigation(tab);
            }
        };
        tab.addObserver(mUrlUpdatedObserver);
    }

    protected void resetCoupon() {
        delete();
        mCoupon = new Coupon();
    }

    /**
     * Enable saving of {@link CouponPersistedTabData}
     */
    protected void enableSaving() {
        mIsTabSaveEnabledSupplier.set(true);
    }

    /**
     * Disable saving of {@link CouponPersistedTabData}. Deletes previously saved {@link
     * CouponPersistedTabData} as well.
     */
    public void disableSaving() {
        mIsTabSaveEnabledSupplier.set(false);
    }

    @VisibleForTesting
    protected void prefetchOnNewNavigation(Tab tab) {
        EndpointFetcher.fetchUsingOAuth(
                (endpointResponse)
                        -> {
                    mCoupon = getCouponFromJSON(endpointResponse.getResponseString());
                    if (mCoupon != null) {
                        enableSaving();
                    }
                },
                Profile.getLastUsedRegularProfile(), PERSISTED_TAB_DATA_ID,
                String.format(Locale.US, ENDPOINT, tab.getUrl().getSpec()), HTTPS_METHOD,
                CONTENT_TYPE, SCOPES, EMPTY_POST_DATA, TIMEOUT_MS, TRAFFIC_ANNOTATION);
    }

    @VisibleForTesting
    public EmptyTabObserver getUrlUpdatedObserverForTesting() {
        return mUrlUpdatedObserver;
    }

    @Override
    public long getTimeToLiveMs() {
        return (int) ONE_HOUR_MS;
    }
}
