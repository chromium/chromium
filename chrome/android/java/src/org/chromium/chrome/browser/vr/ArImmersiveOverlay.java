// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.ScreenOrientationDelegate;
import org.chromium.content_public.browser.ScreenOrientationProvider;
import org.chromium.ui.widget.Toast;

/**
 * Provides a fullscreen overlay for immersive AR mode.
 */
public class ArImmersiveOverlay
        implements SurfaceHolder.Callback2, View.OnTouchListener, ScreenOrientationDelegate {
    private static final String TAG = "ArImmersiveOverlay";
    private static final boolean DEBUG_LOGS = false;

    private ArCoreJavaUtils mArCoreJavaUtils;
    private ChromeActivity mActivity;
    private boolean mSurfaceReportedReady;
    private Integer mRestoreOrientation;
    private boolean mCleanupInProgress;
    private SurfaceUiWrapper mSurfaceUi;

    public void show(
            @NonNull ChromeActivity activity, @NonNull ArCoreJavaUtils caller, boolean useOverlay) {
        if (DEBUG_LOGS) Log.i(TAG, "constructor");
        mArCoreJavaUtils = caller;
        mActivity = activity;

        // Choose a concrete implementation to create a drawable Surface and make it fullscreen.
        // It forwards SurfaceHolder callbacks and touch events to this ArImmersiveOverlay object.
        if (useOverlay) {
            mSurfaceUi = new SurfaceUiCompositor();
        } else {
            mSurfaceUi = new SurfaceUiDialog();
        }
    }

    private interface SurfaceUiWrapper {
        public void onSurfaceVisible();
        public void destroy();
    }

    private class SurfaceUiDialog implements SurfaceUiWrapper, DialogInterface.OnCancelListener {
        private Toast mNotificationToast;
        private Dialog mDialog;
        // Android supports multiple variants of fullscreen applications. Use fully-immersive
        // "sticky" mode without navigation or status bars, and show a toast with a "pull from top
        // and press back button to exit" prompt.
        private static final int VISIBILITY_FLAGS_IMMERSIVE = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        public SurfaceUiDialog() {
            // Create a fullscreen dialog and use its backing Surface for drawing.
            mDialog = new Dialog(mActivity, android.R.style.Theme_NoTitleBar_Fullscreen);
            mDialog.getWindow().setBackgroundDrawable(null);
            mDialog.getWindow().takeSurface(ArImmersiveOverlay.this);
            View view = mDialog.getWindow().getDecorView();
            view.setSystemUiVisibility(VISIBILITY_FLAGS_IMMERSIVE);
            view.setOnTouchListener(ArImmersiveOverlay.this);
            mDialog.setOnCancelListener(this);
            mDialog.getWindow().setLayout(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mDialog.show();
        }

        @Override // SurfaceUiWrapper
        public void onSurfaceVisible() {
            if (mNotificationToast == null) {
                int resId = R.string.immersive_fullscreen_api_notification;
                mNotificationToast = Toast.makeText(mActivity, resId, Toast.LENGTH_LONG);
                mNotificationToast.setGravity(Gravity.TOP | Gravity.CENTER, 0, 0);
            }
            mNotificationToast.show();
        }

        @Override // SurfaceUiWrapper
        public void destroy() {
            if (mNotificationToast != null) {
                mNotificationToast.cancel();
            }
            mDialog.dismiss();
        }

        @Override // DialogInterface.OnCancelListener
        public void onCancel(DialogInterface dialog) {
            if (DEBUG_LOGS) Log.i(TAG, "onCancel");
            cleanupAndExit();
        }
    }

    private class SurfaceUiCompositor extends EmptyTabObserver implements SurfaceUiWrapper {
        private SurfaceView mSurfaceView;
        private CompositorView mCompositorView;

        public SurfaceUiCompositor() {
            mSurfaceView = new SurfaceView(mActivity);
            // Keep the camera layer at "default" Z order. Chrome's compositor SurfaceView is in
            // OverlayVideoMode, putting it in front of that, but behind other non-SurfaceView UI.
            mSurfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);
            mSurfaceView.getHolder().addCallback(ArImmersiveOverlay.this);

            View content = mActivity.getWindow().findViewById(android.R.id.content);
            ViewGroup group = (ViewGroup) content.getParent();
            group.addView(mSurfaceView);

            mCompositorView = mActivity.getCompositorViewHolder().getCompositorView();

            // Enable alpha channel for the compositor and make the background
            // transparent. (A variant of CompositorView::SetOverlayVideoMode.)
            if (DEBUG_LOGS) Log.i(TAG, "calling mCompositorView.setOverlayImmersiveArMode(true)");
            mCompositorView.setOverlayImmersiveArMode(true);

            // Watch for fullscreen exit triggered from JS, this needs to end the session.
            mActivity.getActivityTab().addObserver(this);
        }

        @Override // SurfaceUiWrapper
        public void onSurfaceVisible() {}

        @Override // SurfaceUiWrapper
        public void destroy() {
            mActivity.getActivityTab().removeObserver(this);
            View content = mActivity.getWindow().findViewById(android.R.id.content);
            ViewGroup group = (ViewGroup) content.getParent();
            group.removeView(mSurfaceView);
            mSurfaceView = null;
            mCompositorView.setOverlayImmersiveArMode(false);
        }

        @Override // TabObserver
        public void onExitFullscreenMode(Tab tab) {
            if (DEBUG_LOGS) Log.i(TAG, "onExitFullscreenMode");
            cleanupAndExit();
        }
    }

    @Override // View.OnTouchListener
    public boolean onTouch(View v, MotionEvent ev) {
        // Only forward primary actions, ignore more complex events such as secondary pointer
        // touches. Ignore batching since we're only sending one ray pose per frame.
        if (ev.getAction() == MotionEvent.ACTION_DOWN || ev.getAction() == MotionEvent.ACTION_MOVE
                || ev.getAction() == MotionEvent.ACTION_UP) {
            boolean touching = ev.getAction() != MotionEvent.ACTION_UP;
            if (DEBUG_LOGS) Log.i(TAG, "onTouch touching=" + touching);
            mArCoreJavaUtils.onDrawingSurfaceTouch(touching, ev.getX(0), ev.getY(0));
        }
        return true;
    }

    @Override // ScreenOrientationDelegate
    public boolean canUnlockOrientation(Activity activity, int defaultOrientation) {
        if (mActivity == activity && mRestoreOrientation != null) {
            mRestoreOrientation = defaultOrientation;
            return false;
        }
        return true;
    }

    @Override // ScreenOrientationDelegate
    public boolean canLockOrientation() {
        return false;
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceCreated(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceCreated");
        // Do nothing here, we'll handle setup on the following surfaceChanged.
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceRedrawNeeded(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceRedrawNeeded");
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // The surface may not immediately start out at the expected fullscreen size due to
        // animations or not-yet-hidden navigation bars. WebXR immersive sessions use a fixed-size
        // frame transport that can't be resized, so we need to pick a single size and stick with it
        // for the duration of the session. Use the expected fullscreen size for WebXR frame
        // transport even if the currently-visible part in the surface view is smaller than this. We
        // shouldn't get resize events since we're using FLAG_LAYOUT_STABLE and are locking screen
        // orientation.
        if (mSurfaceReportedReady) {
            int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
            if (DEBUG_LOGS) {
                Log.i(TAG,
                        "surfaceChanged ignoring change to width=" + width + " height=" + height
                                + " rotation=" + rotation);
            }
            return;
        }

        // Need to ensure orientation is locked at this point to avoid race conditions. Save current
        // orientation mode, and then lock current orientation. It's unclear if there's still a risk
        // of races, for example if an orientation change was already in progress at this point but
        // wasn't fully processed yet. In that case the user may need to exit and re-enter the
        // session to get the intended layout.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(this);
        if (mRestoreOrientation == null) {
            mRestoreOrientation = mActivity.getRequestedOrientation();
        }
        mActivity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LOCKED);

        // Display.getRealSize "gets the real size of the display without subtracting any window
        // decor or applying any compatibility scale factors", and "the size is adjusted based on
        // the current rotation of the display". This is what we want since the surface and WebXR
        // frame sizes also use the same current rotation which is now locked, so there's no need to
        // separately adjust for portrait vs landscape modes.
        //
        // While it would be preferable to wait until the surface is at the desired fullscreen
        // resolution, i.e. via mActivity.getFullscreenManager().getPersistentFullscreenMode(), that
        // causes a chicken-and-egg problem for SurfaceUiCompositor mode as used for DOM overlay.
        // Chrome's fullscreen mode is triggered by the Blink side setting an element fullscreen
        // after the session starts, but the session doesn't start until we report the drawing
        // surface being ready (including a configured size), so we use this reported size assuming
        // that's what the fullscreen mode will use.
        Display display = mActivity.getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getRealSize(size);

        if (width < size.x || height < size.y) {
            if (DEBUG_LOGS) {
                Log.i(TAG,
                        "surfaceChanged adjusting size from " + width + "x" + height + " to "
                                + size.x + "x" + size.y);
            }
            width = size.x;
            height = size.y;
        }

        int rotation = mActivity.getWindowManager().getDefaultDisplay().getRotation();
        if (DEBUG_LOGS) {
            Log.i(TAG, "surfaceChanged size=" + width + "x" + height + " rotation=" + rotation);
        }
        mArCoreJavaUtils.onDrawingSurfaceReady(holder.getSurface(), rotation, width, height);
        mSurfaceReportedReady = true;

        // Show the toast with instructions how to exit fullscreen mode now if necessary.
        // Not needed in DOM overlay mode which uses FullscreenHtmlApiHandler to do so.
        mSurfaceUi.onSurfaceVisible();
    }

    @Override // SurfaceHolder.Callback2
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (DEBUG_LOGS) Log.i(TAG, "surfaceDestroyed");
        cleanupAndExit();
    }

    public void cleanupAndExit() {
        if (DEBUG_LOGS) Log.i(TAG, "cleanupAndExit");

        // Avoid duplicate cleanup if we're exiting via ArCoreJavaUtils's endSession.
        // That triggers cleanupAndExit -> remove SurfaceView -> surfaceDestroyed -> cleanupAndExit.
        if (mCleanupInProgress) return;
        mCleanupInProgress = true;

        mSurfaceUi.destroy();

        // The JS app may have put an element into fullscreen mode during the immersive session,
        // even if this wasn't visible to the user. Ensure that we fully exit out of any active
        // fullscreen state on session end to avoid being left in a confusing state.
        if (mActivity.getActivityTab() != null) {
            mActivity.getActivityTab().exitFullscreenMode();
        }

        // Restore orientation.
        ScreenOrientationProvider.getInstance().setOrientationDelegate(null);
        if (mRestoreOrientation != null) mActivity.setRequestedOrientation(mRestoreOrientation);
        mRestoreOrientation = null;

        // The surface is destroyed when exiting via "back" button, but also in other lifecycle
        // situations such as switching apps or toggling the phone's power button. Treat each of
        // these as exiting the immersive session. We need to run the destroy callbacks to ensure
        // consistent state after non-exiting lifecycle events.
        mArCoreJavaUtils.onDrawingSurfaceDestroyed();
    }
}
