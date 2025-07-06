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
class SupervisedUserServiceTestBridge {

    // Substitutes the Supervised User Service with one that allows content filters manipulation.
    static void init(Profile profile) {
        SupervisedUserServiceTestBridgeJni.get().init(profile);
    }

    /** Enables the browser content filters for testing. Call {@link #init} once before this. */
    static void enableBrowserContentFilters(Profile profile) {
        // TODO(crbug.com/429430726): enforce calling init() prior to this.
        SupervisedUserServiceTestBridgeJni.get().enableBrowserContentFilters(profile);
    }

    /** Enables the browser content filters for testing. Call {@link #init} once before this. */
    static void enableSearchContentFilters(Profile profile) {
        // TODO(crbug.com/429430726): enforce calling init() prior to this.
        SupervisedUserServiceTestBridgeJni.get().enableSearchContentFilters(profile);
    }

    @NativeMethods
    interface Natives {
        void init(@JniType("Profile*") Profile profile);
        void enableBrowserContentFilters(@JniType("Profile*") Profile profile);
        void enableSearchContentFilters(@JniType("Profile*") Profile profile);
    }
}
