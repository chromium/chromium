// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Bridge, providing access to the native-side Tracking Protection configuration. */
public class TrackingProtectionBridge {
    private final Profile mProfile;

    public TrackingProtectionBridge(Profile profile) {
        mProfile = profile;
    }

    public @NoticeType int getRequiredNotice() {
        return TrackingProtectionBridgeJni.get().getRequiredNotice(mProfile);
    }

    public void noticeActionTaken(@NoticeType int noticeType, @NoticeAction int action) {
        TrackingProtectionBridgeJni.get().noticeActionTaken(mProfile, noticeType, action);
    }

    public void noticeRequested(@NoticeType int noticeType) {
        TrackingProtectionBridgeJni.get().noticeRequested(mProfile, noticeType);
    }

    public void noticeShown(@NoticeType int noticeType) {
        TrackingProtectionBridgeJni.get().noticeShown(mProfile, noticeType);
    }

    public boolean isOffboarded() {
        return TrackingProtectionBridgeJni.get().isOffboarded(mProfile);
    }

    @NativeMethods
    public interface Natives {
        void noticeRequested(@JniType("Profile*") Profile profile, int noticeType);

        void noticeShown(@JniType("Profile*") Profile profile, int noticeType);

        void noticeActionTaken(@JniType("Profile*") Profile profile, int noticeType, int action);

        int getRequiredNotice(@JniType("Profile*") Profile profile);

        boolean isOffboarded(@JniType("Profile*") Profile profile);
    }
}
