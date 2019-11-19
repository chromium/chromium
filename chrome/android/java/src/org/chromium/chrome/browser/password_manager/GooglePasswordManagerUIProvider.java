// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

/**
 * Shows the Google Password Manager UI if possible.
 */
public interface GooglePasswordManagerUIProvider {
    /**
     * Shows the Google Password Manager UI if possible.
     *
     * @param activity The activity from which to launch the UI to manage passwords.
     * @return Whether showing the Google Password Manager UI was possible or not.
     **/
    boolean showGooglePasswordManager(Activity activity);

    /**
     * Launches the Password Checkup if possible.
     *
     * @param activity The activity from which to launch the Password Checkup.
     * @return Whether launching the Password Checkup was possible or not.
     **/
    boolean launchPasswordCheckup(Activity activity);
}
