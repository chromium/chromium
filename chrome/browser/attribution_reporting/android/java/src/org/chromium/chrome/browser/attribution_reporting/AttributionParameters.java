// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.text.TextUtils;

import androidx.annotation.Nullable;

/** Public constants for the attribution_reporting module. */
public class AttributionParameters {
    private String mSourcePackageName;

    private String mSourceEventId;
    private String mDestination;
    private String mReportTo;
    private long mExpiry;
    private long mEventTime;

    /** @see Method descriptions for the meaning of each of these parameters. */
    public AttributionParameters(String sourcePackageName, String sourceEventId, String destination,
            String reportTo, long expiry) {
        mSourcePackageName = sourcePackageName;

        mSourceEventId = sourceEventId;
        mDestination = destination;
        mReportTo = reportTo;
        mExpiry = expiry;
    }

    /**
     * Constructor for AttributionParameters coming from cached events, where the event time is not
     * the current time.
     *
     * @see Method descriptions for the meaning of each of these parameters.
     */
    public static AttributionParameters forCachedEvent(String sourcePackageName,
            String sourceEventId, String destination, String reportTo, long expiry,
            long eventTime) {
        AttributionParameters params = new AttributionParameters(
                sourcePackageName, sourceEventId, destination, reportTo, expiry);
        params.mEventTime = eventTime;
        return params;
    }

    /** The Package Name of the app that sent this Attribution. */
    public String getSourcePackageName() {
        return mSourcePackageName;
    }

    /** A uint64_t encoded as a string identifying the attribution source. */
    public String getSourceEventId() {
        return mSourceEventId;
    }

    /**
     * If for a click, the origin on which this navigation is intended to finish.
     * If for an impression event, the Origin the impression is for.
     */
    public String getDestination() {
        return mDestination;
    }

    /** An optional origin to send any attribution report to. */
    @Nullable
    public String getReportTo() {
        return mReportTo;
    }

    /**
     * An optional offset in milliseconds from the reporting time, after which the attribution
     * expires. 0 represents an unset expiry time.
     */
    public long getExpiry() {
        return mExpiry;
    }

    /**
     * Returns the time at which the event took place in {@link System#currentTimeMillis()}
     * timebase, or 0 if the event was just received (and not cached).
     */
    public long getEventTime() {
        return mEventTime;
    }

    @Override
    public boolean equals(Object object) {
        if (object == this) return true;
        if (object == null) return false;
        if (getClass() != object.getClass()) return false;
        AttributionParameters other = (AttributionParameters) object;
        return mSourcePackageName.equals(other.mSourcePackageName)
                && mSourceEventId.equals(other.mSourceEventId)
                && mDestination.equals(other.mDestination)
                && TextUtils.equals(mReportTo, other.mReportTo) && mExpiry == other.mExpiry
                && mEventTime == other.mEventTime;
    }

    @Override
    public int hashCode() {
        // Not a valid AttributionParameters.
        if (mSourceEventId == null) return 0;

        // SourceEventId is already fairly likely to be unique, no need to complicate the hashCode.
        return mSourceEventId.hashCode();
    }
}
