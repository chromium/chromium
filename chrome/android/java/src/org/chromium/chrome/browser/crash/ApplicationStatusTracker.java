// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

/** This class updates crash keys when the application state changes. */
public class ApplicationStatusTracker implements ApplicationStatus.ApplicationStateListener {
    private static final String APP_FOREGROUND = "app_foreground";
    private static final String APP_BACKGROUND = "app_background";

    private String mCurrentState;

    @Override
    public void onApplicationStateChange(int newState) {
        ThreadUtils.assertOnUiThread();
        String appStatus;
        // TODO(wnwen): Add foreground service as another state.
        if (isApplicationInForeground(newState)) {
            appStatus = APP_FOREGROUND;
        } else {
            appStatus = APP_BACKGROUND;
        }
        if (!appStatus.equals(mCurrentState)) {
            mCurrentState = appStatus;
            CrashKeys.getInstance().set(CrashKeyIndex.APPLICATION_STATUS, appStatus);
        }
    }

    private static boolean isApplicationInForeground(@ApplicationState int state) {
        return state == ApplicationState.HAS_RUNNING_ACTIVITIES
                || state == ApplicationState.HAS_PAUSED_ACTIVITIES;
    }
}
