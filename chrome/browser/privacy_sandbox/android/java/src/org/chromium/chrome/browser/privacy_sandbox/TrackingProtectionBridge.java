// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.annotations.NativeMethods;

/** Bridge, providing access to the native-side Tracking Protection configuration. */
public class TrackingProtectionBridge {
    public static boolean shouldShowOnboardingNotice() {
        return TrackingProtectionBridgeJni.get().shouldShowOnboardingNotice();
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowOnboardingNotice();
    }
}
