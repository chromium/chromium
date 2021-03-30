// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import androidx.annotation.StringDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Represents the information for one commerce subscription entry.
 */
public class CommerceSubscription {
    @StringDef({CommerceSubscriptionType.UNKNOWN, CommerceSubscriptionType.PRICE_TRACK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CommerceSubscriptionType {
        String UNKNOWN = "UNKNOWN";
        String PRICE_TRACK = "PRICE_TRACK";
    }
    @StringDef({SubscriptionManagementType.UNKNOWN, SubscriptionManagementType.CHROME_MANAGED,
            SubscriptionManagementType.USER_MANAGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SubscriptionManagementType {
        String UNKNOWN = "UNKNOWN";
        String CHROME_MANAGED = "CHROME_MANAGED";
        String USER_MANAGED = "USER_MANAGED";
    }

    private static final Long UNSAVED_SUBSCRIPTION = -1L;

    private final Long mTimestamp;
    private final @CommerceSubscriptionType String mType;
    private final String mTrackingId;
    private final @SubscriptionManagementType String mManagementType;

    CommerceSubscription(@CommerceSubscriptionType String type, String trackingId,
            @SubscriptionManagementType String managementType) {
        this(type, trackingId, managementType, UNSAVED_SUBSCRIPTION);
    }

    CommerceSubscription(@CommerceSubscriptionType String type, String trackingId,
            @SubscriptionManagementType String managementType, Long timestamp) {
        mTrackingId = trackingId;
        mType = type;
        mManagementType = managementType;
        mTimestamp = timestamp;
    }

    Long getTimestamp() {
        return mTimestamp;
    }

    @CommerceSubscriptionType
    String getType() {
        return mType;
    }

    String getTrackingId() {
        return mTrackingId;
    }

    @SubscriptionManagementType
    String getManagementType() {
        return mManagementType;
    }
}
