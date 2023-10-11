// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {

    private boolean mShouldShowOnboardingNotice;

    @Override
    public boolean shouldShowOnboardingNotice() {
        return mShouldShowOnboardingNotice;
    }

    public void setShouldShowOnboardingNotice(boolean shouldShowOnboardingNotice) {
        mShouldShowOnboardingNotice = shouldShowOnboardingNotice;
    }
}
