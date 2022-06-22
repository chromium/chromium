// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;

import java.nio.ByteBuffer;

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

    // TODO(crbug.com/1337470): Implement deserialize & serialize methods.
    @Override
    Supplier<ByteBuffer> getSerializeSupplier() {
        return () -> null;
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
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