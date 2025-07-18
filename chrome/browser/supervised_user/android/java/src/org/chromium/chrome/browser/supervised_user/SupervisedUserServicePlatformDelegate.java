// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;

@NullMarked
class SupervisedUserServicePlatformDelegate {
    /** Closes all open incognito tabs */
    @CalledByNative
    private static void closeIncognitoTabs() {
        // TODO(https://crbug.com/429478269): Switch back to IncognitoTabHostUtils helper once
        // calling things during init is handled for us.
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            host.closeAllIncognitoTabsOnInit();
        }
    }
}
