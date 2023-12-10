// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Intent;
import android.view.Display;
import android.view.View;
import android.view.WindowManager;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.display.DisplayAndroid;

/** Delegate to call into VR. */
public abstract class VrDelegate implements BackPressHandler {
    private static final String TAG = "VrDelegate";
    /* package */ static final boolean DEBUG_LOGS = false;
    /* package */ static final int VR_SYSTEM_UI_FLAGS =
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    public abstract void forceExitVrImmediately();

    public abstract boolean onActivityResultWithNative(int requestCode, int resultCode);

    public abstract void onNativeLibraryAvailable();

    public abstract boolean onBackPressed();

    public abstract void onMultiWindowModeChanged(boolean isInMultiWindowMode);

    public abstract void onActivityShown(Activity activity);

    public abstract void onActivityHidden(Activity activity);

    public abstract void setVrModeEnabled(Activity activity, boolean enabled);

    public abstract boolean isDaydreamReadyDevice();

    public abstract boolean isDaydreamCurrentViewer();

    public void setSystemUiVisibilityForVr(Activity activity) {
        activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        int flags = activity.getWindow().getDecorView().getSystemUiVisibility();
        activity.getWindow().getDecorView().setSystemUiVisibility(flags | VR_SYSTEM_UI_FLAGS);
    }

    /* package */ boolean relaunchOnMainDisplayIfNecessary(Activity activity, Intent intent) {
        boolean onMainDisplay =
                DisplayAndroid.getNonMultiDisplay(activity).getDisplayId()
                        == Display.DEFAULT_DISPLAY;
        // TODO(mthiesse): There's a known race when switching displays on Android O/P that can
        // lead us to actually be on the main display, but our context still thinks it's on
        // the virtual display. This is intended to be fixed for Android Q+, but we can work
        // around the race by explicitly relaunching ourselves to the main display.
        if (!onMainDisplay) {
            Log.i(TAG, "Relaunching Chrome onto the main display.");
            activity.finish();
            activity.startActivity(
                    intent,
                    ApiCompatibilityUtils.createLaunchDisplayIdActivityOptions(
                            Display.DEFAULT_DISPLAY));
            return true;
        }
        return false;
    }

    /* package */ abstract void initAfterModuleInstall();
}
