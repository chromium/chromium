// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.app.Activity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.app.ChromeActivity;

/**
 * Notifies the native power mode arbiter of Chrome app activity changes.
 */
public class ChromePowerModeVoter implements ApplicationStatus.ActivityStateListener {
    @SuppressLint("StaticFieldLeak")
    private static ChromePowerModeVoter sInstance;

    public static ChromePowerModeVoter getInstance() {
        if (sInstance == null) {
            sInstance = new ChromePowerModeVoter();
        }
        return sInstance;
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (!LibraryLoader.getInstance().isInitialized()) return;

        if (activity instanceof ChromeActivity) {
            boolean active = newState == ActivityState.STARTED || newState == ActivityState.RESUMED;
            ChromePowerModeVoterJni.get().onChromeActivityStateChange(active);
        }
    }

    @NativeMethods
    interface Natives {
        void onChromeActivityStateChange(boolean active);
    }
}
