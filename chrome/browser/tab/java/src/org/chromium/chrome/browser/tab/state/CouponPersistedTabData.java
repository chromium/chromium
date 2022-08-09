// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.json.JSONArray;
import org.json.JSONObject;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.CouponPersistedTabData.CouponPersistedTabDataProto;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * {@link PersistedTabData} for Shopping websites with coupons.
 *
 * TODO(crbug.com/1337120): Implement prefetching for CouponPersistedTabData
 * TODO(crbug.com/1337124): Implement clearing CouponPersistedTabData upon new navigation
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
    }

    /**
     * Coupon data type for {@link CouponPersistedTabData}
     */
    // TODO(crbug.com/1335127): Add in fields to hold discount amount.
    public static class Coupon {
        public String couponName;
        public String promoCode;

        /**
         * @param name name of coupon
         * @param code promotional code relating to coupon
         */
        public Coupon(String name, String code) {
            couponName = name;
            promoCode = code;
        }

        public boolean hasCoupon() {
            return couponName != null && promoCode != null;
        }
    }

    /**
     * @return {@link Coupon} relating to the main offer in the page, if available
     */
    public Coupon getCoupon() {
        if (mCoupon == null || !mCoupon.hasCoupon()) {
            return null;
        }
        return mCoupon;
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
            mCoupon = new Coupon(
                    couponPersistedTabDataProto.getName(), couponPersistedTabDataProto.getCode());
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
        try {
            JSONObject jsonObject = new JSONObject(responseString);
            JSONArray discountsArray = jsonObject.optJSONArray(DISCOUNTS_ARRAY);
            coupon = getCouponFromJSON(discountsArray);
        } catch (Exception e) {
            Log.e(TAG, "Error parsing JSON: %s", e.getMessage());
            return null;
        }
        if (coupon == null || !coupon.hasCoupon()) {
            return null;
        }
        return new CouponPersistedTabData(tab, coupon);
    }

    private static Coupon getCouponFromJSON(JSONArray discountsArray) {
        // TODO(crbug.com/1335127): Extract discount amount from JSONArray.
        String name;
        String code;
        try {
            JSONObject discountInfo =
                    discountsArray.optJSONObject(0).optJSONObject(DISCOUNT_INFO_OBJECT);
            name = discountInfo.optString(LONG_TITLE_STRING);
            code = discountInfo.optString(COUPON_CODE_STRING);
        } catch (NullPointerException e) {
            Log.e(TAG, "Error parsing JSON: %s", e.getMessage());
            return null;
        }
        return new Coupon(name, code);
    }

    @Override
    public long getTimeToLiveMs() {
        return (int) ONE_HOUR_MS;
    }
}
