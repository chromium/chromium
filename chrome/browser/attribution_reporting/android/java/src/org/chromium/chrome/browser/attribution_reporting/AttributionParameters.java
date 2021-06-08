// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

/** Public constants for the attribution_reporting module. */
public class AttributionParameters {
    private String mSourcePackageName;

    private String mSourceEventId;
    private String mDestination;
    private String mReportTo;
    private long mExpiry;

    public AttributionParameters(String sourcePackageName, String sourceEventId, String destination,
            String reportTo, long expiry) {
        mSourcePackageName = sourcePackageName;

        mSourceEventId = sourceEventId;
        mDestination = destination;
        mReportTo = reportTo;
        mExpiry = expiry;
    }

    public String getSourcePackageName() {
        return mSourcePackageName;
    }

    public String getSourceEventId() {
        return mSourceEventId;
    }

    public String getDestination() {
        return mDestination;
    }

    public String getReportTo() {
        return mReportTo;
    }

    public long getExpiry() {
        return mExpiry;
    }
}
