// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringDef;

import org.chromium.base.annotations.CalledByNative;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Represents the information for one commerce subscription entry.
 *
 * To add a new SubscriptionType / ManagementType / TrackingIdType:
 * 1. Add the type in this class.
 * 2. Add the corresponding entry in {@link commerce_subscription_db_content.proto} to ensure the
 * storage works correctly.
 */
public class CommerceSubscription {
    @StringDef({CommerceSubscriptionType.TYPE_UNSPECIFIED, CommerceSubscriptionType.PRICE_TRACK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CommerceSubscriptionType {
        String TYPE_UNSPECIFIED = "TYPE_UNSPECIFIED";
        String PRICE_TRACK = "PRICE_TRACK";
    }

    @StringDef({SubscriptionManagementType.TYPE_UNSPECIFIED,
            SubscriptionManagementType.CHROME_MANAGED, SubscriptionManagementType.USER_MANAGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SubscriptionManagementType {
        String TYPE_UNSPECIFIED = "TYPE_UNSPECIFIED";
        String CHROME_MANAGED = "CHROME_MANAGED";
        String USER_MANAGED = "USER_MANAGED";
    }

    @StringDef({TrackingIdType.IDENTIFIER_TYPE_UNSPECIFIED, TrackingIdType.OFFER_ID,
            TrackingIdType.PRODUCT_CLUSTER_ID})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TrackingIdType {
        String IDENTIFIER_TYPE_UNSPECIFIED = "IDENTIFIER_TYPE_UNSPECIFIED";
        String OFFER_ID = "OFFER_ID";
        String PRODUCT_CLUSTER_ID = "PRODUCT_CLUSTER_ID";
    }

    /** The price track offer data specific to price track subscriptions. */
    public static class PriceTrackableOffer {
        public PriceTrackableOffer(@Nullable String offerId, @Nullable String currentPrice,
                @Nullable String countryCode) {
            this.offerId = offerId;
            this.currentPrice = currentPrice;
            this.countryCode = countryCode;
        }
        /** Associated offer id */
        public final String offerId;
        /** Current price upon subscribing */
        public final String currentPrice;
        /** Country code of the offer */
        public final String countryCode;
    }

    public static final long UNSAVED_SUBSCRIPTION = -1L;

    private final long mTimestamp;
    @NonNull
    private final @CommerceSubscriptionType String mType;
    @NonNull
    private final String mTrackingId;
    @NonNull
    private final @SubscriptionManagementType String mManagementType;
    @NonNull
    private final @TrackingIdType String mTrackingIdType;
    @Nullable
    private final PriceTrackableOffer mSeenOffer;

    // TODO(crbug.com/1311754): Clean up this api.
    @Deprecated
    public CommerceSubscription(@NonNull @CommerceSubscriptionType String type,
            @NonNull String trackingId, @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType) {
        this(type, trackingId, managementType, trackingIdType, UNSAVED_SUBSCRIPTION);
    }

    @CalledByNative
    CommerceSubscription(@NonNull @CommerceSubscriptionType String type, @NonNull String trackingId,
            @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType, long timestamp) {
        this(type, trackingId, managementType, trackingIdType, timestamp, null);
    }

    public CommerceSubscription(@NonNull @CommerceSubscriptionType String type,
            @NonNull String trackingId, @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType,
            @Nullable PriceTrackableOffer seenOffer) {
        this(type, trackingId, managementType, trackingIdType, UNSAVED_SUBSCRIPTION, seenOffer);
    }

    private CommerceSubscription(@NonNull @CommerceSubscriptionType String type,
            @NonNull String trackingId, @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType, long timestamp,
            @Nullable PriceTrackableOffer seenOffer) {
        mTrackingId = trackingId;
        mType = type;
        mManagementType = managementType;
        mTrackingIdType = trackingIdType;
        mTimestamp = timestamp;
        mSeenOffer = seenOffer;
    }

    long getTimestamp() {
        return mTimestamp;
    }

    @CommerceSubscriptionType
    String getType() {
        return mType;
    }

    @TrackingIdType
    public String getTrackingIdType() {
        return mTrackingIdType;
    }

    public String getTrackingId() {
        return mTrackingId;
    }

    @SubscriptionManagementType
    public String getManagementType() {
        return mManagementType;
    }

    public PriceTrackableOffer getSeenOffer() {
        return mSeenOffer;
    }

    @CalledByNative
    static List<CommerceSubscription> createSubscriptionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    static CommerceSubscription createSubscriptionAndAddToList(List<CommerceSubscription> list,
            @NonNull @CommerceSubscriptionType String type, @NonNull String trackingId,
            @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType, long timestamp) {
        CommerceSubscription subscription = new CommerceSubscription(
                type, trackingId, managementType, trackingIdType, timestamp);
        list.add(subscription);
        return subscription;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof CommerceSubscription)) {
            return false;
        }
        CommerceSubscription otherSubscription = (CommerceSubscription) other;
        return mManagementType.equals(otherSubscription.getManagementType())
                && mType.equals(otherSubscription.getType())
                && mTrackingId.equals(otherSubscription.getTrackingId())
                && mTrackingIdType.equals(otherSubscription.getTrackingIdType())
                && mTimestamp == otherSubscription.getTimestamp();
    }
}
