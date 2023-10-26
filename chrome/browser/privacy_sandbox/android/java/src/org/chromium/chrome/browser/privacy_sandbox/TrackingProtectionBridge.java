// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.NativeMethods;

/** Bridge, providing access to the native-side Tracking Protection configuration. */
public class TrackingProtectionBridge {

    public static @NoticeType int getRequiredNotice() {
        return TrackingProtectionBridgeJni.get().getRequiredNotice();
    }

    public static void noticeActionTaken(@NoticeType int noticeType, @NoticeAction int action) {
        TrackingProtectionBridgeJni.get().noticeActionTaken(noticeType, action);
    }

    public static void noticeShown(@NoticeType int noticeType) {
        TrackingProtectionBridgeJni.get().noticeShown(noticeType);
    }

    public static boolean isOffboarded() {
        return TrackingProtectionBridgeJni.get().isOffboarded();
    }

    @NativeMethods
    public interface Natives {
        void noticeShown(int noticeType);

        void noticeActionTaken(int noticeType, int action);

        int getRequiredNotice();

        boolean isOffboarded();
    }
}
