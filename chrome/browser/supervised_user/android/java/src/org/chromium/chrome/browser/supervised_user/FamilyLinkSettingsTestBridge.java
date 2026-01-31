// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;

/**
 * Class to expose Family Link Settings Service to Java code. Offers functionalities that fake
 * loading specific settings from remote configurations.
 *
 * <p>This should only be used in tests.
 */
@JNINamespace("supervised_user")
class FamilyLinkSettingsTestBridge {
    /** Set the website filtering behaviour for this user. */
    static void setFilteringBehavior(Profile profile, @FilteringBehavior int setting) {
        FamilyLinkSettingsTestBridgeJni.get().setFilteringBehavior(profile, setting);
    }

    /** Adds the given host to the manuel allowlist or denylist */
    static void setManualFilterForHost(Profile profile, String host, boolean allowlist) {
        FamilyLinkSettingsTestBridgeJni.get().setManualFilterForHost(profile, host, allowlist);
    }

    /** Sets response to the kids management API */
    static void setKidsManagementResponseForTesting(Profile profile, boolean isAllowed) {
        FamilyLinkSettingsTestBridgeJni.get()
                .setKidsManagementResponseForTesting(profile, isAllowed);
    }

    @NativeMethods
    interface Natives {
        void setFilteringBehavior(@JniType("Profile*") Profile profile, int setting);

        void setManualFilterForHost(
                @JniType("Profile*") Profile profile, String host, boolean allowlist);

        void setKidsManagementResponseForTesting(
                @JniType("Profile*") Profile profile, boolean siteIsAllowed); // IN-TEST
    }
}
