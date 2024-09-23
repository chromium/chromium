// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.components.embedder_support.view.ContentView;

public class FullscreenHtmlApiHandlerCompat extends FullscreenHtmlApiHandlerBase
        implements View.OnApplyWindowInsetsListener {

    // TAG length is limited to 20 characters, so we cannot use full class name:
    private static final String TAG = "FullscreenApiCompat";

    /**
     * Constructs the legacy handler that will manage the UI transitions from the HTML fullscreen
     * API.
     *
     * @param activity The activity that supports fullscreen.
     * @param areControlsHidden Supplier of a flag indicating if browser controls are hidden.
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be true for
     *     Activities that are not always fullscreen.
     */
    public FullscreenHtmlApiHandlerCompat(
            Activity activity,
            ObservableSupplier<Boolean> areControlsHidden,
            boolean exitFullscreenOnStop) {
        super(activity, areControlsHidden, exitFullscreenOnStop);
    }

    // View.OnApplyWindowInsetsListener

    @NonNull
    @Override
    public WindowInsets onApplyWindowInsets(@NonNull View v, @NonNull WindowInsets insets) {
        if (mTabInFullscreen != null && getPersistentFullscreenMode()) {
            mHandler.sendEmptyMessageDelayed(
                    MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS, ANDROID_CONTROLS_SHOW_DURATION_MS);
        }
        return insets;
    }

    // FullscreenHtmlApiHandlerBase

    @Override
    protected void setContentView(ContentView contentView) {
        ContentView oldContentView = getContentView();
        if (contentView == oldContentView) return;
        if (oldContentView != null) {
            oldContentView.setOnApplyWindowInsetsListener(null);
        }
        super.setContentView(contentView);
        if (contentView != null) {
            contentView.setOnApplyWindowInsetsListener(this);
        }
    }

    @Override
    void hideSystemBars(View contentView, FullscreenOptions fullscreenOptions) {
        boolean showNavigationBar =
                fullscreenOptions != null && fullscreenOptions.showNavigationBar;
        boolean showStatusBar = fullscreenOptions != null && fullscreenOptions.showStatusBar;

        assert !(showNavigationBar && showStatusBar)
                : "Cannot enter fullscreen with both status and navigation bars visible!";

        WindowInsetsControllerCompat windowInsetsController = getWindowInsetsController();

        windowInsetsController.setSystemBarsBehavior(
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        setLayoutFullscreen(/* not used */ contentView);
        if (!showStatusBar && !showNavigationBar) {
            windowInsetsController.hide(WindowInsetsCompat.Type.systemBars());
        } else if (!showNavigationBar) {
            windowInsetsController.hide(WindowInsetsCompat.Type.navigationBars());
        } else /* !showStatusBar */ {
            windowInsetsController.hide(WindowInsetsCompat.Type.statusBars());
        }
    }

    @Override
    void showSystemBars(View contentView) {
        WindowInsetsControllerCompat windowInsetsController = getWindowInsetsController();

        windowInsetsController.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_DEFAULT);
        // SystemBars() includes both the navigation and status bars, as well as captions and some
        // system overlays.
        windowInsetsController.show(WindowInsetsCompat.Type.systemBars());
        unsetLayoutFullscreen(/* not used */ contentView);
    }

    @Override
    void adjustSystemBarsInFullscreenMode(View contentView, FullscreenOptions fullscreenOptions) {
        boolean showNavigationBar =
                fullscreenOptions != null && fullscreenOptions.showNavigationBar;
        boolean showStatusBar = fullscreenOptions != null && fullscreenOptions.showStatusBar;

        assert !(showNavigationBar && showStatusBar)
                : "Cannot enter fullscreen with both status and navigation bars visible!";

        WindowInsetsControllerCompat windowInsetsController = getWindowInsetsController();

        windowInsetsController.setSystemBarsBehavior(
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        setLayoutFullscreen(/* not used */ contentView);

        if (!showStatusBar && !showNavigationBar) {
            // SystemBars() includes both the navigation and status bars, as well as captions and
            // some system overlays.
            windowInsetsController.hide(WindowInsetsCompat.Type.systemBars());
        } else if (!showNavigationBar) {
            windowInsetsController.hide(WindowInsetsCompat.Type.navigationBars());
            windowInsetsController.show(WindowInsetsCompat.Type.statusBars());
        } else /* !showStatusBar */ {
            windowInsetsController.hide(WindowInsetsCompat.Type.statusBars());
            windowInsetsController.show(WindowInsetsCompat.Type.navigationBars());
        }
    }

    @Override
    boolean isStatusBarHidden(View contentView) {
        return !getWindowInsets(contentView).isVisible(WindowInsetsCompat.Type.statusBars());
    }

    @Override
    boolean isNavigationBarHidden(View contentView) {
        return !getWindowInsets(contentView).isVisible(WindowInsetsCompat.Type.navigationBars());
    }

    @Override
    boolean isLayoutFullscreen(View contentView) {
        return !mActivity.getWindow().getDecorView().getFitsSystemWindows();
    }

    @Override
    boolean isLayoutHidingNavigation(View contentView) {
        return isLayoutFullscreen(contentView);
    }

    @Override
    void hideNavigationBar(View contentView) {
        getWindowInsetsController().hide(WindowInsetsCompat.Type.navigationBars());
    }

    // TODO(crbug.com/41492646): Coordinate usage of #setDecorFitsSystemWindows
    @Override
    void setLayoutFullscreen(View contentView) {
        // Avoid setting this on automotive, as automotive devices are inconsistent in their
        // support for drawing edge-to-edge.
        if (BuildInfo.getInstance().isAutomotive) {
            return;
        }
        // TODO(crbug.com/41492929): Account for floating windows.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), false);
    }

    // TODO(crbug.com/41492646): Coordinate usage of #setDecorFitsSystemWindows
    @Override
    void unsetLayoutFullscreen(View contentView) {
        // TODO(crbug.com/41492929): Account for floating windows.
        WindowCompat.setDecorFitsSystemWindows(mActivity.getWindow(), true);
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
        Log.i(TAG, "enterFullscreen, systemUiVisibility=" + getSystemUIVisibility(contentView));
    }

    @Override
    void logEnterFullscreenOptions(FullscreenOptions fullscreenOptions) {
        Log.i(TAG, "enterFullscreen, options=" + fullscreenOptions.toString());
    }

    @Override
    void logExitFullscreen(View contentView) {
        Log.i(TAG, "exitFullscreen, systemUiVisibility=" + getSystemUIVisibility(contentView));
    }

    @Override
    void logHandlerUnsetFullscreenLayout(View contentView) {
        Log.i(
                TAG,
                "handleMessage clear fullscreen flag, systemUiVisibility="
                        + getSystemUIVisibility(contentView));
    }

    @Override
    void logHandleMessageHideSystemBars(View contentView) {
        Log.i(
                TAG,
                "handleMessage set flags, systemUiVisibility="
                        + getSystemUIVisibility(contentView));
    }

    private WindowInsetsCompat getWindowInsets(View contentView) {
        return WindowInsetsCompat.toWindowInsetsCompat(
                contentView.getRootWindowInsets(), contentView);
    }

    private WindowInsetsControllerCompat getWindowInsetsController() {
        Window window = mActivity.getWindow();
        return WindowCompat.getInsetsController(window, window.getDecorView());
    }

    private String getSystemUIVisibility(View contentView) {
        boolean statusBarVisibility = !isStatusBarHidden(contentView);
        boolean navBarVisibility = !isNavigationBarHidden(contentView);
        boolean systemBarVisibility =
                getWindowInsets(contentView).isVisible(WindowInsetsCompat.Type.systemBars());
        boolean decorViewFitsWindow = isLayoutFullscreen(contentView);
        boolean transientBars =
                getWindowInsetsController().getSystemBarsBehavior()
                        == WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;

        return String.format(
                "{statusbar=%b, navbar=%b, systemBars=%b, decorViewFitsWindow=%b,"
                        + " transientBars=%b}",
                statusBarVisibility,
                navBarVisibility,
                systemBarVisibility,
                decorViewFitsWindow,
                transientBars);
    }
}
