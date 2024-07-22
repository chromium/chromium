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

    public @NoticeType int getRequiredNotice(@SurfaceType int surface) {
        return TrackingProtectionBridgeJni.get().getRequiredNotice(mProfile, surface);
    }

    public void noticeActionTaken(
            @SurfaceType int surface, @NoticeType int noticeType, @NoticeAction int action) {
        TrackingProtectionBridgeJni.get().noticeActionTaken(mProfile, surface, noticeType, action);
    }

    public void noticeShown(@SurfaceType int surface, @NoticeType int noticeType) {
        TrackingProtectionBridgeJni.get().noticeShown(mProfile, surface, noticeType);
    }

    public boolean shouldRunUILogic(@SurfaceType int surface) {
        return TrackingProtectionBridgeJni.get().shouldRunUILogic(mProfile, surface);
    }

    @NativeMethods
    public interface Natives {
        void noticeShown(@JniType("Profile*") Profile profile, int surface, int noticeType);

        void noticeActionTaken(
                @JniType("Profile*") Profile profile, int surface, int noticeType, int action);

        int getRequiredNotice(@JniType("Profile*") Profile profile, int surface);

        boolean shouldRunUILogic(@JniType("Profile*") Profile profile, int surface);
    }
}
