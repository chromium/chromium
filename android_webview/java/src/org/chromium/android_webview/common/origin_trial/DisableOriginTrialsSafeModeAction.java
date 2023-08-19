// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.origin_trial;

import androidx.annotation.NonNull;

import org.chromium.android_webview.common.SafeModeAction;

/**
 * A {@link SafeModeAction} to disable all origin trials
 */
public class DisableOriginTrialsSafeModeAction implements SafeModeAction {
    private static final String TAG = "WebViewSafeMode";

    private static boolean sDisableOriginTrials;

    // This ID should not be changed or reused.
    public static final String ID = "disable_origin_trials";

    @Override
    @NonNull
    public String getId() {
        return ID;
    }

    @Override
    public boolean execute() {
        sDisableOriginTrials = true;
        return true;
    }

    public static boolean isDisableOriginTrialsEnabled() {
        return sDisableOriginTrials;
    }
}
