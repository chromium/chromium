// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

/**
 * A discrete point in time representing a single piece of browsing activity for a given
 * fully-qualified domain name (FQDN).
 */
public class WebsiteEvent {
    @IntDef({EventType.START, EventType.STOP})
    public @interface EventType {
        int START = 1;
        int STOP = 2;
    }

    private final long mTimestamp;
    private final String mFqdn;
    private final @EventType int mEventType;

    public WebsiteEvent(long timestamp, @NonNull String fqdn, @EventType int eventType) {
        mTimestamp = timestamp;
        assert fqdn != null;
        mFqdn = fqdn;
        mEventType = eventType;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public String getFqdn() {
        return mFqdn;
    }

    public @EventType int getType() {
        return mEventType;
    }
}