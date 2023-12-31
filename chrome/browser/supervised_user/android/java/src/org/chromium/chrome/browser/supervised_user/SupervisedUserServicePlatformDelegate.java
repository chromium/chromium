// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;

class SupervisedUserServicePlatformDelegate {
    /** Closes all open incognito tabs */
    @CalledByNative
    private static void closeIncognitoTabs() {
        IncognitoTabHostUtils.closeAllIncognitoTabs();
    }
}
