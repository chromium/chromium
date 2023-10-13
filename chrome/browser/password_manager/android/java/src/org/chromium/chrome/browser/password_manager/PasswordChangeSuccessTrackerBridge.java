// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.jni_zero.NativeMethods;

import org.chromium.url.GURL;

/**
 * Class handling the communication with the C++ part of the password change
 * success tracker feature. It forwards messages to and from its C++
 * counterpart.
 */
public class PasswordChangeSuccessTrackerBridge {
    // Key for the extra that carries the username of a credential for which a
    // manual password change flow has been launched from GmsCore.
    public static final String EXTRA_MANUAL_CHANGE_USERNAME_KEY =
            "org.chromium.chrome.browser.password_change.username";

    /**
     * Register the start of a manual password change flow. Notifies the
     * password change success tracker.
     * @param url The URL associated with the credential that is to be changed.
     * @param username The username of the credential that is to be changed.
     */
    public static void onManualPasswordChangeStarted(GURL url, String username) {
        PasswordChangeSuccessTrackerBridgeJni.get().onManualPasswordChangeStarted(url, username);
    }

    /**
     * C++ method signatures.
     */
    @NativeMethods
    public interface Natives {
        void onManualPasswordChangeStarted(GURL url, String username);
    }
}
