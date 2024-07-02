// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.Nullable;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Java equivalent of magic_stack_bridge.cc */
class MagicStackBridge {
    private static ProfileKeyedMap<MagicStackBridge> sProfileMap;

    private final Profile mProfile;

    static MagicStackBridge getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }
        return sProfileMap.getForProfile(profile, MagicStackBridge::new);
    }

    MagicStackBridge(Profile profile) {
        mProfile = profile;
    }

    @Nullable
    MagicStackEntry getModuleToShow() {
        return MagicStackBridgeJni.get().getModuleToShow(mProfile);
    }

    void dismissActiveModule() {
        MagicStackBridgeJni.get().dismissActiveModule(mProfile);
    }

    @NativeMethods
    interface Native {
        @JniType("std::optional<MenuNotificationEntry>")
        @Nullable
        MagicStackEntry getModuleToShow(@JniType("Profile*") Profile profile);

        void dismissActiveModule(@JniType("Profile*") Profile profile);
    }
}
