// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Exposes the supervised_user_preferences.h to Java tests. Using this class can be useful to
 * behaviors triggered by external events (account changes, devices changes).
 */
@JNINamespace("supervised_user")
class SupervisedUserServiceTestBridge {

    /** Enables the browser content filters for testing. Call {@link #init} once before this. */
    static void enableBrowserContentFilters() {
        SupervisedUserServiceTestBridgeJni.get().enableBrowserContentFilters();
    }

    /** Enables the browser content filters for testing. Call {@link #init} once before this. */
    static void enableSearchContentFilters() {
        SupervisedUserServiceTestBridgeJni.get().enableSearchContentFilters();
    }

    @NativeMethods
    interface Natives {
        void enableBrowserContentFilters();

        void enableSearchContentFilters();
    }
}
