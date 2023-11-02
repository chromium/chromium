// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.test;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;

/**
 * Class to expose supervised user settings to Java code.
 *
 * This should only be used in tests.
 */
public class SupervisedUserSettingsBridge {
    /** Set the website filtering behaviour for this user. */
    static void setFilteringBehavior(Profile profile, @FilteringBehavior int setting) {
        SupervisedUserSettingsBridgeJni.get().setFilteringBehavior(profile, setting);
    }

    @NativeMethods
    interface Natives {
        void setFilteringBehavior(Profile profile, int setting);
    }
}
