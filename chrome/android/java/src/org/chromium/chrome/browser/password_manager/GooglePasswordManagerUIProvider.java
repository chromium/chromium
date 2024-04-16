// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

/** Shows the Google Password Manager UI if possible. */
public interface GooglePasswordManagerUIProvider {
    /**
     * Shows the Google Password Manager UI if possible.
     *
     * @param activity The activity from which to launch the UI to manage passwords.
     * @return Whether showing the Google Password Manager UI was possible or not.
     *     TODO(crbug.com/41425234): Remove once downstream implementation is removed.
     */
    default boolean showGooglePasswordManager(Activity activity) {
        return false;
    }

    /**
     * Launches the Password Checkup if possible.
     *
     * @param activity The activity from which to launch the Password Checkup.
     * @return Whether launching the Password Checkup was possible or not.
     **/
    boolean launchPasswordCheckup(Activity activity);
}
