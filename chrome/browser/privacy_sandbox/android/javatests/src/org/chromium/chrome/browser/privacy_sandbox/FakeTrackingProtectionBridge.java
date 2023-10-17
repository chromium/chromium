// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {
    private boolean mShouldShowOnboardingNotice;
    private Integer mLastOnboardingNoticeAction;
    private boolean mOnboardingNoticeShown;

    @Override
    public boolean shouldShowOnboardingNotice() {
        return mShouldShowOnboardingNotice;
    }

    @Override
    public void onboardingNoticeShown() {
        mOnboardingNoticeShown = true;
    }

    @Override
    public void onboardingNoticeActionTaken(@NoticeAction int action) {
        mLastOnboardingNoticeAction = action;
    }

    public void setShouldShowOnboardingNotice(boolean shouldShowOnboardingNotice) {
        mShouldShowOnboardingNotice = shouldShowOnboardingNotice;
    }

    public boolean wasOnboardingNoticeShown() {
        return mOnboardingNoticeShown;
    }

    public Integer getLastOnboardingNoticeAction() {
        return mLastOnboardingNoticeAction;
    }
}
