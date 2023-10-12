// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {
    private boolean mShouldShowOnboardingNotice;
    private Integer mLastNoticeAction;
    private boolean mNoticeShown;

    @Override
    public boolean shouldShowOnboardingNotice() {
        return mShouldShowOnboardingNotice;
    }

    @Override
    public void noticeShown() {
        mNoticeShown = true;
    }

    @Override
    public void noticeActionTaken(@NoticeAction int action) {
        mLastNoticeAction = action;
    }

    public void setShouldShowOnboardingNotice(boolean shouldShowOnboardingNotice) {
        mShouldShowOnboardingNotice = shouldShowOnboardingNotice;
    }

    public boolean wasNoticeShown() {
        return mNoticeShown;
    }

    public Integer getLastNoticeAction() {
        return mLastNoticeAction;
    }
}
