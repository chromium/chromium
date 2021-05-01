// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;

import static androidx.core.view.WindowInsetsCompat.Type.systemBars;
import static androidx.core.view.WindowInsetsControllerCompat.BEHAVIOR_SHOW_BARS_BY_SWIPE;
import static androidx.core.view.WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.view.View;
import android.view.Window;

import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.display_cutout.ActivityDisplayCutoutModeSupplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;
import org.chromium.ui.base.WindowAndroid;

import javax.inject.Inject;

/**
 * Allows to enter and exit immersive mode in TWAs and WebAPKs.
 */
@ActivityScope
public class ImmersiveModeController implements WindowFocusChangedObserver, DestroyObserver {
    private static final int ENTER_IMMERSIVE_MODE_ON_WINDOW_FOCUS_DELAY_MILLIS = 300;
    private static final int RESTORE_IMMERSIVE_MODE_DELAY_MILLIS = 3000;

    private final Activity mActivity;
    private final ActivityDisplayCutoutModeSupplier mCutoutSupplier =
            new ActivityDisplayCutoutModeSupplier();
    private final Handler mHandler = new Handler();
    private final Runnable mUpdateImmersiveFlagsRunnable = this::updateImmersiveFlags;

    private boolean mInImmersiveMode;

    @Inject
    public ImmersiveModeController(ActivityLifecycleDispatcher lifecycleDispatcher,
            Activity activity, WindowAndroid window) {
        mActivity = activity;
        lifecycleDispatcher.register(this);

        mCutoutSupplier.attach(window.getUnownedUserDataHost());
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
        Window window = mActivity.getWindow();
        View decor = window.getDecorView();

        WindowInsetsControllerCompat insetsController =
                WindowCompat.getInsetsController(window, decor);
        assert insetsController != null : "Decor View isn't attached to the Window.";

        if (sticky) {
            insetsController.setSystemBarsBehavior(BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        } else {
            insetsController.setSystemBarsBehavior(BEHAVIOR_SHOW_BARS_BY_SWIPE);
        }

        // When we enter immersive mode for the first time, register a
        // SystemUiVisibilityChangeListener that restores immersive mode. This is necessary
        // because user actions like focusing a keyboard will break out of immersive mode.
        decor.setOnSystemUiVisibilityChangeListener(
                newFlags -> postSetImmersiveFlags(RESTORE_IMMERSIVE_MODE_DELAY_MILLIS));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // In order to avoid a flicker during launch, set the display cutout mode now (vs
            // waiting for DisplayCutoutController to set the mode).
            window.getAttributes().layoutInDisplayCutoutMode = layoutInDisplayCutoutMode;
            mCutoutSupplier.set(layoutInDisplayCutoutMode);
        }

        postSetImmersiveFlags(0);
    }

    /**
     * Exits immersive mode.
     */
    public void exitImmersiveMode() {
        if (!mInImmersiveMode) return;

        mInImmersiveMode = false;
        mHandler.removeCallbacks(mUpdateImmersiveFlagsRunnable);
        updateImmersiveFlags();
        mCutoutSupplier.set(LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    private void postSetImmersiveFlags(int delayInMills) {
        if (!mInImmersiveMode) return;

        mHandler.removeCallbacks(mUpdateImmersiveFlagsRunnable);
        mHandler.postDelayed(mUpdateImmersiveFlagsRunnable, delayInMills);
    }

    private void updateImmersiveFlags() {
        Window window = mActivity.getWindow();
        View decor = window.getDecorView();

        WindowInsetsControllerCompat insetsController =
                WindowCompat.getInsetsController(window, decor);

        assert insetsController != null : "Decor View isn't attached to the Window.";

        if (mInImmersiveMode) {
            insetsController.hide(systemBars());
        } else {
            insetsController.show(systemBars());
        }

        WindowCompat.setDecorFitsSystemWindows(window, !mInImmersiveMode);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (hasFocus && mInImmersiveMode) {
            postSetImmersiveFlags(ENTER_IMMERSIVE_MODE_ON_WINDOW_FOCUS_DELAY_MILLIS);
        }
    }

    @Override
    public void onDestroy() {
        mHandler.removeCallbacks(mUpdateImmersiveFlagsRunnable);
        mCutoutSupplier.destroy();
    }
}
