// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.profiles.Profile;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {
    private Integer mLastNoticeAction;
    private boolean mNoticeShown;
    private boolean mIsOffboarded;

    private @NoticeType int mNoticeType;

    @Override
    public void noticeShown(Profile profile, int noticeType) {
        mNoticeShown = true;
    }

    @Override
    public void noticeActionTaken(Profile profile, int noticeType, int action) {
        mLastNoticeAction = action;
    }

    @Override
    public @NoticeType int getRequiredNotice(Profile profile) {
        return mNoticeType;
    }

    @Override
    public boolean isOffboarded(Profile profile) {
        return mIsOffboarded;
    }

    public void setRequiredNotice(@NoticeType int noticeType) {
        mNoticeType = noticeType;
    }

    public boolean wasNoticeShown() {
        return mNoticeShown;
    }

    public Integer getLastNoticeAction() {
        return mLastNoticeAction;
    }

    public void setIsOffboarded(boolean isOffboarded) {
        mIsOffboarded = isOffboarded;
    }
}
