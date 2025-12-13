// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Bridge, providing access to the native-side TrackingProtectionSettings API. */
@NullMarked
public class TrackingProtectionSettingsBridge {
    public static void maybeSetRollbackPrefsModeB(Profile profile) {
        TrackingProtectionSettingsBridgeJni.get().maybeSetRollbackPrefsModeB(profile);
    }

    @NativeMethods
    public interface Natives {
        void maybeSetRollbackPrefsModeB(Profile profile);
    }
}
