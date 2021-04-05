// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.NonNull;
import androidx.annotation.StringDef;

import org.chromium.base.annotations.CalledByNative;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Represents the information for one commerce subscription entry.
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

    @StringDef({TrackingIdType.IDENTIFIER_TYPE_UNSPECIFIED, TrackingIdType.OFFER_ID})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TrackingIdType {
        String IDENTIFIER_TYPE_UNSPECIFIED = "IDENTIFIER_TYPE_UNSPECIFIED";
        String OFFER_ID = "OFFER_ID";
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

    CommerceSubscription(@NonNull @CommerceSubscriptionType String type, @NonNull String trackingId,
            @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType) {
        this(type, trackingId, managementType, trackingIdType, UNSAVED_SUBSCRIPTION);
    }

    @CalledByNative
    CommerceSubscription(@NonNull @CommerceSubscriptionType String type, @NonNull String trackingId,
            @NonNull @SubscriptionManagementType String managementType,
            @NonNull @TrackingIdType String trackingIdType, long timestamp) {
        mTrackingId = trackingId;
        mType = type;
        mManagementType = managementType;
        mTrackingIdType = trackingIdType;
        mTimestamp = timestamp;
    }

    long getTimestamp() {
        return mTimestamp;
    }

    @CommerceSubscriptionType
    String getType() {
        return mType;
    }

    @TrackingIdType
    String getTrackingIdType() {
        return mTrackingIdType;
    }

    String getTrackingId() {
        return mTrackingId;
    }

    @SubscriptionManagementType
    String getManagementType() {
        return mManagementType;
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
