// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Manages setting the initial screen orientation for the custom tab. Delays all screen orientation
 * requests till the activity translucency is removed.
 */
@NullMarked
public class CustomTabOrientationController {
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final int mLockScreenOrientation;

    public CustomTabOrientationController(
            ActivityWindowAndroid windowAndroid,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mActivityWindowAndroid = windowAndroid;
        mLockScreenOrientation = intentDataProvider.getDefaultOrientation();
    }

    public void setCanControlOrientation(boolean inAppMode) {
        int defaultWebOrientation =
                inAppMode ? mLockScreenOrientation : ScreenOrientationLockType.DEFAULT;
        ScreenOrientationProvider.getInstance()
                .setOverrideDefaultOrientation(
                        mActivityWindowAndroid, (byte) defaultWebOrientation);

        ScreenOrientationProvider.getInstance().unlockOrientation(mActivityWindowAndroid);
    }
}
