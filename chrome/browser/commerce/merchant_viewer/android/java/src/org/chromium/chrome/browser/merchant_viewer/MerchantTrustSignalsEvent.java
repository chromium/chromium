// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.jni_zero.CalledByNative;

import java.util.ArrayList;
import java.util.List;

/** Represents the information for one merchant trust signal event entry. */
public class MerchantTrustSignalsEvent {
    private final String mKey;
    private final long mTimestamp;

    @CalledByNative
    MerchantTrustSignalsEvent(String key, long timestamp) {
        mKey = key;
        mTimestamp = timestamp;
    }

    @CalledByNative
    static List<MerchantTrustSignalsEvent> createEventList() {
        return new ArrayList<>();
    }

    @CalledByNative
    static MerchantTrustSignalsEvent createEventAndAddToList(
            List<MerchantTrustSignalsEvent> list, String key, long timestamp) {
        MerchantTrustSignalsEvent event = new MerchantTrustSignalsEvent(key, timestamp);
        list.add(event);
        return event;
    }

    String getKey() {
        return mKey;
    }

    long getTimestamp() {
        return mTimestamp;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof MerchantTrustSignalsEvent)) {
            return false;
        }
        MerchantTrustSignalsEvent otherEvent = (MerchantTrustSignalsEvent) other;
        return mKey.equals(otherEvent.getKey()) && mTimestamp == otherEvent.getTimestamp();
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + (mKey == null ? 0 : mKey.hashCode());
        result = 31 * result + (int) mTimestamp;
        return result;
    }
}
