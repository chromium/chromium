// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.ui.base.WindowAndroid;

/**
 * The bridge regroups methods invoked by native code to interact with Android Signin UI.
 */
final class SigninBridge {
    /**
     * Opens account management screen.
     */
    @CalledByNative
    private static void openAccountManagementScreen(
            WindowAndroid windowAndroid, @GAIAServiceType int gaiaServiceType) {
        ThreadUtils.assertOnUiThread();
        final Context context = windowAndroid.getContext().get();
        if (context != null) {
            AccountManagementFragment.openAccountManagementScreen(context, gaiaServiceType);
        }
    }

    private SigninBridge() {}
}
