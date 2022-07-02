// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.proto.CouponPersistedTabData.CouponPersistedTabDataProto;

import java.nio.ByteBuffer;
import java.util.Locale;

/**
 * {@link PersistedTabData} for Shopping websites with coupons.
 *
 * TODO(crbug.com/1335109): Implement service integration for CouponPersistedTabData
 * TODO(crbug.com/1337120): Implement prefetching for CouponPersistedTabData
 * TODO(crbug.com/1337124): Implement clearing CouponPersistedTabData upon new navigation
 */
public class CouponPersistedTabData extends PersistedTabData {
    private static final String TAG = "COPTD";
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
    }

    /**
     * @return {@link Coupon} relating to the main offer in the page, if available
     */
    public Coupon getCoupon() {
        if (mCoupon.promoCode.isEmpty()) {
            return null;
        }
        return mCoupon;
    }

    @Override
    Supplier<ByteBuffer> getSerializeSupplier() {
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
     * Acquire {@link CouponPersistedTabData} for a {@link Tab}
     * @param tab {@link Tab} that {@link CouponPersistedTabData} is acquired for
     * @param callback {@link Callback} that {@link CouponPersistedTabData} is passed back in
     */
    public static void from(Tab tab, Callback<CouponPersistedTabData> callback) {
        // An empty string as the code field will return a null coupon when calling getCoupon()
        // so the UI will always be turned off until this method has been further implemented.
        callback.onResult(new CouponPersistedTabData(tab, new Coupon("Test", "")));
    }
}