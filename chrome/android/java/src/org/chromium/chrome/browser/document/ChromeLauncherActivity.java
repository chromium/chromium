// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.os.Bundle;
import android.os.StrictMode;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class ChromeLauncherActivity extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Third-party code adds disk access to Activity.onCreate. http://crbug.com/619824
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        TraceEvent.begin("ChromeLauncherActivity.onCreate");
        try {
            super.onCreate(savedInstanceState);

            if (VrModuleProvider.getIntentDelegate().isVrIntent(getIntent())) {
                // We need to turn VR mode on as early as possible in the intent handling flow to
                // avoid brightness flickering when handling VR intents.
                VrModuleProvider.getDelegate().setVrModeEnabled(this, true);
            }

            @LaunchIntentDispatcher.Action
            int dispatchAction = LaunchIntentDispatcher.dispatch(this, getIntent());
            switch (dispatchAction) {
                case LaunchIntentDispatcher.Action.FINISH_ACTIVITY:
                    finish();
                    break;
                case LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK:
                    ApiCompatibilityUtils.finishAndRemoveTask(this);
                    break;
                default:
                    assert false : "Intent dispatcher finished with action " + dispatchAction
                                   + ", finishing anyway";
                    finish();
                    break;
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
            TraceEvent.end("ChromeLauncherActivity.onCreate");
        }
    }
}
