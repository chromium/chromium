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
    private final Profile mProfile;

    public TrackingProtectionSettingsBridge(Profile profile) {
        mProfile = profile;
    }

    public boolean isIpProtectionDisabledForEnterprise() {
        return TrackingProtectionSettingsBridgeJni.get()
                .isIpProtectionDisabledForEnterprise(mProfile);
    }

    @NativeMethods
    public interface Natives {
        boolean isIpProtectionDisabledForEnterprise(Profile profile);
    }
}
