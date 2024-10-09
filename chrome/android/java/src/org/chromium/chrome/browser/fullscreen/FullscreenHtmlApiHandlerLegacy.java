// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.app.Activity;
import android.view.View;
import android.view.WindowManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.embedder_support.view.ContentView;

public class FullscreenHtmlApiHandlerLegacy extends FullscreenHtmlApiHandlerBase
        implements View.OnSystemUiVisibilityChangeListener {

    // TAG length is limited to 20 characters, so we cannot use full class name:
    private static final String TAG = "FullscreenApiLegacy";

    /**
     * Constructs the legacy handler that will manage the UI transitions from the HTML fullscreen
     * API.
     *
     * @param activity The activity that supports fullscreen.
     * @param areControlsHidden Supplier of a flag indicating if browser controls are hidden.
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be true for
     *     Activities that are not always fullscreen.
     */
    public FullscreenHtmlApiHandlerLegacy(
            Activity activity,
            ObservableSupplier<Boolean> areControlsHidden,
            boolean exitFullscreenOnStop) {
        super(activity, areControlsHidden, exitFullscreenOnStop);
    }

    // View.OnSystemUiVisibilityChangeListener

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if (mTabInFullscreen == null || !getPersistentFullscreenMode()) return;
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS, ANDROID_CONTROLS_SHOW_DURATION_MS);
    }

    // FullscreenHtmlApiHandlerBase

    @Override
    protected void setContentView(ContentView contentView) {
        ContentView oldContentView = getContentView();
        if (contentView == oldContentView) return;
        if (oldContentView != null) {
            oldContentView.removeOnSystemUiVisibilityChangeListener(this);
        }
        super.setContentView(contentView);
        if (contentView != null) {
            contentView.addOnSystemUiVisibilityChangeListener(this);
        }
    }

    @Override
    void hideSystemBars(View contentView, FullscreenOptions fullscreenOptions) {
        setSystemUiVisibility(
                contentView,
                applyEnterFullscreenUIFlags(
                        contentView.getSystemUiVisibility(), fullscreenOptions));
    }

    @Override
    void showSystemBars(View contentView) {
        setSystemUiVisibility(
                contentView, applyExitFullscreenUIFlags(contentView.getSystemUiVisibility()));
    }

    @Override
    void adjustSystemBarsInFullscreenMode(View contentView, FullscreenOptions fullscreenOptions) {
        setSystemUiVisibility(
                contentView,
                applyEnterFullscreenUIFlags(
                        applyExitFullscreenUIFlags(contentView.getSystemUiVisibility()),
                        fullscreenOptions));
    }

    @Override
    boolean isStatusBarHidden(View contentView) {
        return (contentView.getSystemUiVisibility() & SYSTEM_UI_FLAG_FULLSCREEN)
                == SYSTEM_UI_FLAG_FULLSCREEN;
    }

    @Override
    boolean isNavigationBarHidden(View contentView) {
        return (contentView.getSystemUiVisibility() & SYSTEM_UI_FLAG_HIDE_NAVIGATION)
                == SYSTEM_UI_FLAG_HIDE_NAVIGATION;
    }

    @Override
    boolean isLayoutFullscreen(View contentView) {
        return (contentView.getSystemUiVisibility() & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
                == SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
    }

    @Override
    boolean isLayoutHidingNavigation(View contentView) {
        return (contentView.getSystemUiVisibility() & SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
                == SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
    }

    @Override
    void hideNavigationBar(View contentView) {
        setSystemUiVisibility(
                contentView,
                contentView.getSystemUiVisibility() | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    @Override
    void setLayoutFullscreen(View contentView) {
        setSystemUiVisibility(
                contentView,
                contentView.getSystemUiVisibility() | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    void unsetLayoutFullscreen(View contentView) {
        setSystemUiVisibility(
                contentView,
                contentView.getSystemUiVisibility() & ~SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    }

    @Override
    void setTranslucentStatusBar() {
        setWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
    }

    @Override
    void unsetTranslucentStatusBar() {
        clearWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
    }

    @Override
    void logEnterFullscreen(View contentView) {
        Log.i(TAG, "enterFullscreen, systemUiVisibility=" + contentView.getSystemUiVisibility());
    }

    @Override
    void logEnterFullscreenOptions(FullscreenOptions fullscreenOptions) {
        Log.i(TAG, "enterFullscreen, options=" + fullscreenOptions.toString());
    }

    @Override
    void logExitFullscreen(View contentView) {
        Log.i(TAG, "exitFullscreen, systemUiVisibility=" + contentView.getSystemUiVisibility());
    }

    @Override
    void logHandlerUnsetFullscreenLayout(View contentView) {
        Log.i(
                TAG,
                "handleMessage clear fullscreen flag, systemUiVisibility="
                        + contentView.getSystemUiVisibility());
    }

    @Override
    void logHandleMessageHideSystemBars(View contentView) {
        Log.i(
                TAG,
                "handleMessage set flags, systemUiVisibility="
                        + contentView.getSystemUiVisibility());
    }

    private void setSystemUiVisibility(View contentView, int systemUiVisibility) {
        if (!BuildInfo.getInstance().isAutomotive) {
            contentView.setSystemUiVisibility(systemUiVisibility);
        }
    }

    /**
     * Returns system ui flags to enable fullscreen mode based on the current options.
     *
     * @return fullscreen flags to be applied to system UI visibility.
     */
    private int applyEnterFullscreenUIFlags(
            int systemUiVisibility, FullscreenOptions fullscreenOptions) {
        boolean showNavigationBar =
                fullscreenOptions != null && fullscreenOptions.showNavigationBar;
        boolean showStatusBar = fullscreenOptions != null && fullscreenOptions.showStatusBar;

        int flags = SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (!showStatusBar && !showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
        }

        if (!showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            flags |= SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        }

        if (!showStatusBar) {
            flags |= SYSTEM_UI_FLAG_FULLSCREEN;
            flags |= SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        }

        return flags | systemUiVisibility;
    }

    /**
     * Returns system ui flags with any flags that might have been set during
     * applyEnterFullscreenUIFlags masked off.
     *
     * @return fullscreen flags to be applied to system UI visibility.
     */
    private static int applyExitFullscreenUIFlags(int systemUiVisibility) {
        int maskOffFlags =
                SYSTEM_UI_FLAG_LOW_PROFILE
                        | SYSTEM_UI_FLAG_FULLSCREEN
                        | SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        return systemUiVisibility & ~maskOffFlags;
    }
}
