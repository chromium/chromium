// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.Build;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashscreenObserver;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.base.ActivityWindowAndroid;

import javax.inject.Inject;

/**
 * Manages setting the initial screen orientation for the custom tab.
 * Delays all screen orientation requests till the activity translucency is removed.
 */
@ActivityScope
public class CustomTabOrientationController {
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private int mLockScreenOrientation;

    @Inject
    public CustomTabOrientationController(
            ActivityWindowAndroid activityWindowAndroid,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mActivityWindowAndroid = activityWindowAndroid;

        mLockScreenOrientation = intentDataProvider.getDefaultOrientation();
    }

    /**
     * Delays screen orientation requests if the activity window's initial translucency and the
     * Android OS version requires it.
     * Should be called:
     * - Prior to pre inflation startup occurring.
     * - Only if the splash screen is shown for the activity.
     */
    public void delayOrientationRequestsIfNeeded(
            SplashController splashController, boolean isWindowInitiallyTranslucent) {
        // Setting the screen orientation while the activity is translucent throws an exception on
        // O (but not on O MR1).
        if (!isWindowInitiallyTranslucent || Build.VERSION.SDK_INT != Build.VERSION_CODES.O) return;

        ScreenOrientationProvider.getInstance().delayOrientationRequests(mActivityWindowAndroid);

        splashController.addObserver(
                new SplashscreenObserver() {
                    @Override
                    public void onTranslucencyRemoved() {
                        ScreenOrientationProvider.getInstance()
                                .runDelayedOrientationRequests(mActivityWindowAndroid);
                    }

                    @Override
                    public void onSplashscreenHidden(long startTimestamp, long endTimestamp) {}
                });
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
