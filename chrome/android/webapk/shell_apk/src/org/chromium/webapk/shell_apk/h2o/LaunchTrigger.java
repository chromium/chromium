// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

/**
 * Controls when to launch the WebAPK for {@link SplashActivity}.
 *
 * <p>Executes the provided Runnable when all of {@link #onSplashScreenReady}, {@link #onWillLaunch}
 * and {@link #onHostBrowserSelected} have been called. The provided Runnable is only called once,
 * but this can be reset by calling {@link #reset}.
 */
public class LaunchTrigger {
    private final Runnable mCallback;

    // Variables that determine whether we are ready to encode the splash screen.
    private boolean mSplashScreenReady;
    private boolean mWillLaunch;
    private boolean mHostBrowserSelected;
    private boolean mLaunchingOrLaunched;

    public LaunchTrigger(Runnable callback) {
        mCallback = callback;
    }

    // Methods that could trigger the splash screen encoding.
    public void onSplashScreenReady() {
        mSplashScreenReady = true;
        maybeTrigger();
    }

    public void onWillLaunch() {
        mWillLaunch = true;
        maybeTrigger();
    }

    public void onHostBrowserSelected() {
        mHostBrowserSelected = true;
        maybeTrigger();
    }

    public void reset() {
        mLaunchingOrLaunched = false;
        mHostBrowserSelected = false;
        mWillLaunch = false;
    }

    private void maybeTrigger() {
        if (!mHostBrowserSelected || !mSplashScreenReady || !mWillLaunch || mLaunchingOrLaunched) {
            return;
        }
        mLaunchingOrLaunched = true;

        mCallback.run();
    }
}
