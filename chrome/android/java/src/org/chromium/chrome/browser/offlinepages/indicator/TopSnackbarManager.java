// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manager for one-off snackbar showing at the top of activity.
 */
public class TopSnackbarManager implements OnClickListener, ApplicationStatus.ActivityStateListener,
                                           BrowserControlsStateProvider.Observer {
    private final Handler mDismissSnackbarHandler;
    private final Runnable mDismissSnackbarRunnable = new Runnable() {
        @Override
        public void run() {
            dismissSnackbar(false);
        }
    };

    private Activity mActivity;
    private Snackbar mSnackbar;
    private TopSnackbarView mSnackbarView;
    private BrowserControlsManager mBrowserControlsManager;

    public TopSnackbarManager() {
        mDismissSnackbarHandler = new Handler();
    }

    @Override
    public void onClick(View v) {
        dismissSnackbar(true);
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.PAUSED || newState == ActivityState.STOPPED) {
            dismissSnackbar(false);
        }
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        // When the top toolbar offset changes, dismiss the top snackbar. Ideally we want to move
        // the top snackbar together with the top toolbar, but they can't be made sync because they
        // are drawn in different layers (C++ vs Android native).
        dismissSnackbar(false);
    }

    /**
     * Shows a snackbar at the top of the given activity.
     */
    public void show(Snackbar snackbar, Activity activity, WindowAndroid windowAndroid,
            @Nullable Supplier<BrowserControlsManager> browserControlsManagerSupplier) {
        if (mSnackbar != null) return;

        @ActivityState
        int state = ApplicationStatus.getStateForActivity(activity);
        if (state != ActivityState.STARTED && state != ActivityState.RESUMED) return;

        mActivity = activity;
        mSnackbar = snackbar;

        mSnackbarView = new TopSnackbarView(
                activity, this, mSnackbar, windowAndroid, browserControlsManagerSupplier);
        mSnackbarView.show();
        mSnackbarView.announceforAccessibility();
        mDismissSnackbarHandler.removeCallbacks(mDismissSnackbarRunnable);
        mDismissSnackbarHandler.postDelayed(mDismissSnackbarRunnable, mSnackbar.getDuration());

        if (browserControlsManagerSupplier != null && browserControlsManagerSupplier.hasValue()) {
            mBrowserControlsManager = browserControlsManagerSupplier.get();
            mBrowserControlsManager.addObserver(this);
        }

        ApplicationStatus.registerStateListenerForActivity(this, activity);
    }

    /**
     * Dismisses the snackbar if it is shown.
     */
    public void hide() {
        dismissSnackbar(false);
    }

    private void dismissSnackbar(boolean byAction) {
        if (mSnackbar == null) return;

        if (byAction) {
            mSnackbar.getController().onAction(null);
        } else {
            mSnackbar.getController().onDismissNoAction(null);
        }

        ApplicationStatus.unregisterActivityStateListener(this);

        if (mBrowserControlsManager != null) {
            mBrowserControlsManager.removeObserver(this);
            mBrowserControlsManager = null;
        }

        mDismissSnackbarHandler.removeCallbacks(mDismissSnackbarRunnable);
        if (mSnackbarView != null) {
            mSnackbarView.dismiss();
            mSnackbarView = null;
        }
        mSnackbar = null;
    }

    @VisibleForTesting
    boolean isShowing() {
        return mSnackbar != null;
    }
}
