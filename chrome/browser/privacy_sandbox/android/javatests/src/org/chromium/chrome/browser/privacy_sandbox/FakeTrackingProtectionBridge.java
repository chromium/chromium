// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {
    private Integer mLastNoticeAction;
    private boolean mNoticeShown;
    private boolean mNoticeRequested;
    private boolean mIsOffboarded;

    private @NoticeType int mNoticeType;

    @Override
    public void noticeRequested(int noticeType) {
        mNoticeRequested = true;
    }

    @Override
    public void noticeShown(int noticeType) {
        mNoticeShown = true;
    }

    @Override
    public void noticeActionTaken(int noticeType, int action) {
        mLastNoticeAction = action;
    }

    @Override
    public @NoticeType int getRequiredNotice() {
        return mNoticeType;
    }

    @Override
    public boolean isOffboarded() {
        return mIsOffboarded;
    }

    public void setRequiredNotice(@NoticeType int noticeType) {
        mNoticeType = noticeType;
    }

    public boolean wasNoticeRequested() {
        return mNoticeRequested;
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
