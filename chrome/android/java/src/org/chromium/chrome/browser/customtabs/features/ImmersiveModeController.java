// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.view.View;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;

import javax.inject.Inject;

/**
 * Allows to enter and exit immersive mode in TWAs and WebAPKs.
 */
@ActivityScope
public class ImmersiveModeController implements WindowFocusChangedObserver, Destroyable {

    private static final int ENTER_IMMERSIVE_MODE_ON_WINDOW_FOCUS_DELAY_MILLIS = 300;
    private static final int RESTORE_IMMERSIVE_MODE_DELAY_MILLIS = 3000;

    // As per https://developer.android.com/training/system-ui/immersive.
    private static final int IMMERSIVE_MODE_UI_FLAGS = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION // hide nav bar
            | View.SYSTEM_UI_FLAG_FULLSCREEN // hide status bar
            | View.SYSTEM_UI_FLAG_LOW_PROFILE
            | View.SYSTEM_UI_FLAG_IMMERSIVE;

    private static final int IMMERSIVE_STICKY_MODE_UI_FLAGS = IMMERSIVE_MODE_UI_FLAGS
            & ~View.SYSTEM_UI_FLAG_IMMERSIVE
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

    private final Activity mActivity;
    private final Handler mHandler = new Handler();
    private final Runnable mSetImmersiveFlagsRunnable = this::setImmersiveFlags;

    private int mImmersiveFlags;
    private boolean mInImmersiveMode;

    @Inject
    public ImmersiveModeController(ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity) {
        mActivity = activity;
        lifecycleDispatcher.register(this);
    }

    /**
     * Sets activity's decor view into an immersive mode and ensures it stays that way.
     *
     * @param layoutInDisplayCutoutMode Integer defining how to deal with cutouts, see
     * {@link android.view.WindowManager.LayoutParams#layoutInDisplayCutoutMode} and
     * https://developer.android.com/guide/topics/display-cutout
     *
     * @param sticky Whether {@link View#SYSTEM_UI_FLAG_IMMERSIVE} or
     * {@link View#SYSTEM_UI_FLAG_IMMERSIVE_STICKY} should be used.
     * See https://developer.android.com/training/system-ui/immersive#sticky-immersive
     */
    public void enterImmersiveMode(int layoutInDisplayCutoutMode, boolean sticky) {
        if (mInImmersiveMode) return;

        mInImmersiveMode = true;
        View decor = mActivity.getWindow().getDecorView();
        mImmersiveFlags = sticky ? IMMERSIVE_STICKY_MODE_UI_FLAGS : IMMERSIVE_MODE_UI_FLAGS;

        // When we enter immersive mode for the first time, register a
        // SystemUiVisibilityChangeListener that restores immersive mode. This is necessary
        // because user actions like focusing a keyboard will break out of immersive mode.
        decor.setOnSystemUiVisibilityChangeListener(newFlags -> {
            if ((newFlags | mImmersiveFlags) != newFlags) {
                postSetImmersiveFlags(RESTORE_IMMERSIVE_MODE_DELAY_MILLIS);
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mActivity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                    layoutInDisplayCutoutMode;
        }

        postSetImmersiveFlags(0);
    }

    private void postSetImmersiveFlags(int delayInMills) {
        if (!mInImmersiveMode) return;

        mHandler.removeCallbacks(mSetImmersiveFlagsRunnable);
        mHandler.postDelayed(mSetImmersiveFlagsRunnable, delayInMills);
    }

    private void setImmersiveFlags() {
        View decor = mActivity.getWindow().getDecorView();
        int currentFlags = decor.getSystemUiVisibility();
        int desiredFlags = currentFlags | mImmersiveFlags;
        if (currentFlags != desiredFlags) {
            decor.setSystemUiVisibility(desiredFlags);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (hasFocus && mInImmersiveMode) {
            postSetImmersiveFlags(ENTER_IMMERSIVE_MODE_ON_WINDOW_FOCUS_DELAY_MILLIS);
        }
    }

    /**
     * Exits immersive mode.
     */
    public void exitImmersiveMode() {
        if (!mInImmersiveMode) return;
        mInImmersiveMode = false;
        mHandler.removeCallbacks(mSetImmersiveFlagsRunnable);
        View decor = mActivity.getWindow().getDecorView();
        int currentFlags = decor.getSystemUiVisibility();
        int desiredFlags = currentFlags & ~mImmersiveFlags;
        if (currentFlags != desiredFlags) {
            decor.setSystemUiVisibility(desiredFlags);
        }
    }

    @Override
    public void destroy() {
        mHandler.removeCallbacks(mSetImmersiveFlagsRunnable);
    }
}
