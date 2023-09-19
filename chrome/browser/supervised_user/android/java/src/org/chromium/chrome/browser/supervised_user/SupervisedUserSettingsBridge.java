// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;

/**
 * Class to expose supervised user settings to Java code.
 *
 * This should only be used in tests.
 */
class SupervisedUserSettingsBridge {
    /** Set the website filtering behaviour for this user. */
    static void setFilteringBehavior(Profile profile, @FilteringBehavior int setting) {
        SupervisedUserSettingsBridgeJni.get().setFilteringBehavior(profile, setting);
    }

    /** Adds the given host to the manuel allowlist or denylist*/
    static void setManualFilterForHost(Profile profile, String host, boolean allowlist) {
        SupervisedUserSettingsBridgeJni.get().setManualFilterForHost(profile, host, allowlist);
    }

    @NativeMethods
    interface Natives {
        void setFilteringBehavior(Profile profile, int setting);
        void setManualFilterForHost(Profile profile, String host, boolean allowlist);
    }
}
