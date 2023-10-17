// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.NativeMethods;

/** Bridge, providing access to the native-side Tracking Protection configuration. */
public class TrackingProtectionBridge {
    public static boolean shouldShowOnboardingNotice() {
        return TrackingProtectionBridgeJni.get().shouldShowOnboardingNotice();
    }

    public static void onboardingNoticeActionTaken(@NoticeAction int action) {
        TrackingProtectionBridgeJni.get().onboardingNoticeActionTaken(action);
    }

    public static void onboardingNoticeShown() {
        TrackingProtectionBridgeJni.get().onboardingNoticeShown();
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowOnboardingNotice();

        void onboardingNoticeShown();

        void onboardingNoticeActionTaken(int action);
    }
}
