// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.JniType;

import org.chromium.chrome.browser.profiles.Profile;

/** Java implementation of TrackingProtectionBridge for testing. */
public class FakeTrackingProtectionBridge implements TrackingProtectionBridge.Natives {
    private Integer mLastNoticeAction;
    private boolean mNoticeShown;

    private @NoticeType int mNoticeType;

    @Override
    public void noticeShown(Profile profile, int surface, int noticeType) {
        mNoticeShown = true;
    }

    @Override
    public void noticeActionTaken(Profile profile, int surface, int noticeType, int action) {
        mLastNoticeAction = action;
    }

    @Override
    public @NoticeType int getRequiredNotice(Profile profile, int surface) {
        return mNoticeType;
    }

    @Override
    public boolean shouldRunUILogic(@JniType("Profile*") Profile profile, int surface) {
        return true;
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
}
