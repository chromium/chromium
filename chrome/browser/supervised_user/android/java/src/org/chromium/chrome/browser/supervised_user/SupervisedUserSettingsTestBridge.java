// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.superviseduser.FilteringBehavior;

/**
 * Class to expose supervised user settings to Java code.
 *
 * This should only be used in tests.
 */
class SupervisedUserSettingsTestBridge {
    /** Set the website filtering behaviour for this user. */
    static void setFilteringBehavior(Profile profile, @FilteringBehavior int setting) {
        SupervisedUserSettingsTestBridgeJni.get().setFilteringBehavior(profile, setting);
    }

    /** Adds the given host to the manuel allowlist or denylist*/
    static void setManualFilterForHost(Profile profile, String host, boolean allowlist) {
        SupervisedUserSettingsTestBridgeJni.get().setManualFilterForHost(profile, host, allowlist);
    }

    /** Sets response to the kids management API */
    static void setKidsManagementResponseForTesting(Profile profile, boolean isAllowed) {
        SupervisedUserSettingsTestBridgeJni.get()
                .setKidsManagementResponseForTesting(profile, isAllowed);
    }

    /** Sets response to the safe sites API */
    static void setSafeSearchResponseForTesting(Profile profile, boolean isAllowed) {
        SupervisedUserSettingsTestBridgeJni.get()
                .setSafeSearchResponseForTesting(profile, isAllowed);
    }

    /** Sets up the TestUrlLoaderFactoryHelper, to be used in tests */
    static void setUpTestUrlLoaderFactoryHelper() {
        SupervisedUserSettingsTestBridgeJni.get().setUpTestUrlLoaderFactoryHelper();
    }

    /** Tears down up the TestUrlLoaderFactoryHelper, to be used in tests */
    static void tearDownTestUrlLoaderFactoryHelper() {
        SupervisedUserSettingsTestBridgeJni.get().tearDownTestUrlLoaderFactoryHelper();
    }

    @NativeMethods
    interface Natives {
        void setFilteringBehavior(@JniType("Profile*") Profile profile, int setting);

        void setManualFilterForHost(
                @JniType("Profile*") Profile profile, String host, boolean allowlist);

        void setKidsManagementResponseForTesting(
                @JniType("Profile*") Profile profile, boolean siteIsAllowed); // IN-TEST

        void setSafeSearchResponseForTesting(
                @JniType("Profile*") Profile profile, boolean siteIsAllowed); // IN-TEST

        void setUpTestUrlLoaderFactoryHelper();

        void tearDownTestUrlLoaderFactoryHelper();
    }
}
