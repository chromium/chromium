// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Exposes the supervised_user_preferences.h to Java tests. Using this class can be useful to
 * behaviors triggered by external events (account changes, devices changes).
 */
@JNINamespace("supervised_user")
class SupervisedUserPreferencesTestBridge {

    /**
     * Enables browser content filters for the given profile. In production, the native call
     * supervised_user::EnableBrowserContentFilters proxied from here is also triggered when the
     * supervised_user::ContentFiltersObserverBridge is notified of a change in the Android's secure
     * settings, which configure the browser content filters. Effectively, this method simulates
     * user changing the browser content filters in the Android Settings.
     */
    static void enableBrowserContentFilters(Profile profile) {
        SupervisedUserPreferencesTestBridgeJni.get().enableBrowserContentFilters(profile);
    }

    @NativeMethods
    interface Natives {
        void enableBrowserContentFilters(@JniType("Profile*") Profile profile);
    }
}
