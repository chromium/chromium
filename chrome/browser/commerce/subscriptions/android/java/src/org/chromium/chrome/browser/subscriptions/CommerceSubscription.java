// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.annotations.CalledByNative;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Represents the information for one commerce subscription entry.
 */
public class CommerceSubscription {
    @IntDef({CommerceSubscriptionType.SUBSCRIPTION_TYPE_UNSPECIFIED,
            CommerceSubscriptionType.PRICE_TRACK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CommerceSubscriptionType {
        int SUBSCRIPTION_TYPE_UNSPECIFIED = 0;
        int PRICE_TRACK = 1;
    }

    @IntDef({SubscriptionManagementType.MANAGE_TYPE_UNSPECIFIED,
            SubscriptionManagementType.CHROME_MANAGED, SubscriptionManagementType.USER_MANAGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SubscriptionManagementType {
        int MANAGE_TYPE_UNSPECIFIED = 0;
        int CHROME_MANAGED = 1;
        int USER_MANAGED = 2;
    }

    @IntDef({TrackingIdType.TRACKING_TYPE_UNSPECIFIED, TrackingIdType.OFFER_ID})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TrackingIdType {
        int TRACKING_TYPE_UNSPECIFIED = 0;
        int OFFER_ID = 1;
    }

    private static final long UNSAVED_SUBSCRIPTION = -1L;

    private final long mTimestamp;
    private final @CommerceSubscriptionType int mType;
    @NonNull
    private final String mTrackingId;
    private final @SubscriptionManagementType int mManagementType;
    private final @TrackingIdType int mTrackingIdType;

    CommerceSubscription(@CommerceSubscriptionType int type, @NonNull String trackingId,
            @SubscriptionManagementType int managementType, @TrackingIdType int trackingIdType) {
        this(type, trackingId, managementType, trackingIdType, UNSAVED_SUBSCRIPTION);
    }

    @CalledByNative
    CommerceSubscription(@CommerceSubscriptionType int type, @NonNull String trackingId,
            @SubscriptionManagementType int managementType, @TrackingIdType int trackingIdType,
            long timestamp) {
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
    int getType() {
        return mType;
    }

    @TrackingIdType
    int getTrackingIdType() {
        return mTrackingIdType;
    }

    String getTrackingId() {
        return mTrackingId;
    }

    @SubscriptionManagementType
    int getManagementType() {
        return mManagementType;
    }

    @CalledByNative
    static List<CommerceSubscription> createSubscriptionList() {
        return new ArrayList<>();
    }

    @CalledByNative
    static CommerceSubscription createSubscriptionAndAddToList(List<CommerceSubscription> list,
            @CommerceSubscriptionType int type, @NonNull String trackingId,
            @SubscriptionManagementType int managementType, @TrackingIdType int trackingIdType,
            long timestamp) {
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
        return mManagementType == otherSubscription.getManagementType()
                && mType == otherSubscription.getType()
                && mTrackingId.equals(otherSubscription.getTrackingId())
                && mTrackingIdType == otherSubscription.getTrackingIdType()
                && mTimestamp == otherSubscription.getTimestamp();
    }
}
