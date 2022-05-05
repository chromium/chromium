// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Class for starting a password change flow in Autofill Assistant. */
public class PasswordChangeLauncher {

    @CalledByNative
    public static void start(
            WindowAndroid windowAndroid, GURL origin, String username, boolean skipLogin) {

    }

}
