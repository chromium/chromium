// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utitily class for launching the password leak check.
 */
public class PasswordCheckupLauncher {

    @CalledByNative
    private static void launchCheckupInAccountWithWindowAndroid(
            String checkupUrl, WindowAndroid windowAndroid) {
    }

    @CalledByNative
    // TODO(crbug.com/1311952): Merge with launchLocalCheckupFromPhishGuardWarningDialog.
    private static void launchLocalCheckup(WindowAndroid windowAndroid) {

    }

    @CalledByNative
    // TODO(crbug.com/1311952): Merge with launchLocalCheckup.
    private static void launchLocalCheckupFromPhishGuardWarningDialog(WindowAndroid windowAndroid) {

    }

    @CalledByNative
    private static void launchCheckupInAccountWithActivity(String checkupUrl, Activity activity) {

    }

}
