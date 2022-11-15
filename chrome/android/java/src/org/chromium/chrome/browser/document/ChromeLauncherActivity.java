// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import com.ark.browser.ArkBrowserActivity;
import com.google.android.material.color.DynamicColors;
import com.zpj.toast.ZToast;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.theme.ThemeUtils;

/**
 * Dispatches incoming intents to the appropriate activity based on the current configuration and
 * Intent fired.
 */
public class ChromeLauncherActivity extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Third-party code adds disk access to Activity.onCreate. http://crbug.com/619824
        TraceEvent.begin("ChromeLauncherActivity.onCreate");
        super.onCreate(savedInstanceState);

        // TODO(https://crbug.com/1225066): Figure out a scalable way to apply overlays to
        // activities like this.
        applyThemeOverlays();

        ZToast.error("ChromeLauncherActivity");

        Intent intent = new Intent(this, ArkBrowserActivity.class);
        startActivity(intent);
        finish();

//        @LaunchIntentDispatcher.Action
//        int dispatchAction = LaunchIntentDispatcher.dispatch(this, getIntent());
//        switch (dispatchAction) {
//            case LaunchIntentDispatcher.Action.FINISH_ACTIVITY:
//                finish();
//                break;
//            case LaunchIntentDispatcher.Action.FINISH_ACTIVITY_REMOVE_TASK:
//                this.finishAndRemoveTask();
//                break;
//            default:
//                assert false : "Intent dispatcher finished with action " + dispatchAction
//                               + ", finishing anyway";
//                finish();
//                break;
//        }
        TraceEvent.end("ChromeLauncherActivity.onCreate");
    }

    private void applyThemeOverlays() {
        setTheme(R.style.ColorOverlay_ChromiumAndroid);

        // The effect of this activity's theme is currently limited to CCTs, so we should only apply
        // dynamic colors when we enable them everywhere.
        if (ThemeUtils.ENABLE_FULL_DYNAMIC_COLORS.getValue()) {
            DynamicColors.applyIfAvailable(this);
        }
    }
}
