// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

/** Collection of Chrome branding-related data read from/written to storage. */
class BrandingInfo {
    public static final BrandingInfo EMPTY =
            new BrandingInfo(null, BrandingChecker.BRANDING_TIME_NOT_FOUND, null);

    /** The last time any branding/mismatch notification UI was shown. */
    public final long lastShowTime;

    public final MismatchNotificationData mimData;

    private @BrandingDecision Integer mDecision; // May be updated, thus not final

    public BrandingInfo(
            @BrandingDecision Integer decision,
            long lastShowTime,
            MismatchNotificationData mimData) {
        mDecision = decision;
        this.lastShowTime = lastShowTime;
        this.mimData = mimData;
    }

    /** Sets branding decision. */
    public void setDecision(@BrandingDecision Integer decision) {
        mDecision = decision;
    }

    /** Returns branding decision. */
    public Integer getDecision() {
        return mDecision;
    }
}
