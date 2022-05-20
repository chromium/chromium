// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/**
 * The bridge regroups methods invoked by native code to interact with Android Signin UI.
 */
final class SigninBridge {

    @CalledByNative
    private static void launchSigninActivity(
            WindowAndroid windowAndroid, @SigninAccessPoint int accessPoint) {
        // TODO
    }

    @CalledByNative
    private static void openAccountManagementScreen(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceType) {

    }

    @CalledByNative
    static void openAccountPickerBottomSheet(WindowAndroid windowAndroid, String continueUrl) {

    }

    private SigninBridge() {}
}
