// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.shadows;

import android.content.Intent;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;

/** Shadow implementation of ExternalNavigationDelegateImpl. */
@Implements(ExternalNavigationDelegateImpl.class)
public class ShadowExternalNavigationDelegateImpl {

    private static boolean sWillChromeHandleIntent;

    public static void setWillChromeHandleIntent(boolean value) {
        sWillChromeHandleIntent = value;
    }

    @Implementation
    public static boolean willChromeHandleIntent(Intent intent, boolean matchDefaultOnly) {
        return sWillChromeHandleIntent;
    }
}
