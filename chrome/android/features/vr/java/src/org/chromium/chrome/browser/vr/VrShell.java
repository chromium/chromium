// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr")
public class VrShell extends GvrLayout implements SurfaceHolder.Callback {
    private static final String TAG = "VrShellImpl";

    private final Activity mActivity;
    private final VrShellDelegate mDelegate;
    private final View.OnTouchListener mTouchListener;
    private final TabModelSelector mTabModelSelector;
    private final TabCreatorManager mTabCreatorManager;
    private final WindowAndroid mWindowAndroid;

    private long mNativeVrShell;

    private View mPresentationView;

    // The tab that holds the main WebContents.
    private Tab mTab;

    private VrWindowAndroid mContentVrWindowAndroid;

    private boolean mReprojectedRendering;

    private Boolean mPaused;

    private OnDispatchTouchEventCallback mOnDispatchTouchEventForTesting;
    private Runnable mOnVSyncPausedForTesting;

    /**
     * A struct-like object for registering UI operations during tests.
     */
    @VisibleForTesting
    public static class UiOperationData {
        // The UiTestOperationType of this operation.
        public int actionType;
        // The callback to run when the operation completes.
        public Runnable resultCallback;
        // The timeout of the operation.
        public int timeoutMs;
        // The UserFriendlyElementName to perform the operation on.
        public int elementName;
        // The desired visibility status of the element.
        public boolean visibility;
    }

    public VrShell(Activity activity, VrShellDelegate delegate, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager, WindowAndroid windowAndroid, Tab tab) {
        super(activity);
        mActivity = activity;
        mDelegate = delegate;
        mTabModelSelector = tabModelSelector;
        mTabCreatorManager = tabCreatorManager;
        mWindowAndroid = windowAndroid;
        mTab = tab;

        mReprojectedRendering = setAsyncReprojectionEnabled(true);
        if (mReprojectedRendering) {
            // No need render to a Surface if we're reprojected. We'll be rendering with surfaceless
            // EGL.
            mPresentationView = new FrameLayout(mActivity);

            // This can show up behind popups on standalone devices, so make sure it's black.
            mPresentationView.setBackgroundColor(Color.BLACK);

            // Only enable sustained performance mode when Async reprojection decouples the app
            // framerate from the display framerate.
            AndroidCompat.setSustainedPerformanceMode(mActivity, true);
        } else {
            if (VrShellDelegate.isDaydreamCurrentViewer()) {
                // We need Async Reprojection on when entering VR browsing, because otherwise our
                // GL context will be lost every time we're hidden, like when we go to the dashboard
                // and come back.
                // TODO(mthiesse): Supporting context loss turned out to be hard. We should consider
                // spending more effort on supporting this in the future if it turns out to be
                // important.
                Log.e(TAG, "Could not turn async reprojection on for Daydream headset.");
                throw new VrShellDelegate.VrUnsupportedException();
            }
            SurfaceView surfaceView = new SurfaceView(mActivity);
            surfaceView.getHolder().addCallback(this);
            mPresentationView = surfaceView;
        }

        DisplayAndroid primaryDisplay = DisplayAndroid.getNonMultiDisplay(activity);

        mContentVrWindowAndroid = new VrWindowAndroid(mActivity, primaryDisplay);
        reparentTab(mContentVrWindowAndroid);

        setPresentationView(mPresentationView);

        getUiLayout().setCloseButtonListener(mDelegate.getVrCloseButtonListener());
        getUiLayout().setSettingsButtonListener(mDelegate.getVrSettingsButtonListener());

        mTouchListener = new View.OnTouchListener() {
            @Override
            @SuppressLint("ClickableViewAccessibility")
            public boolean onTouch(View v, MotionEvent event) {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    VrShellJni.get().onTriggerEvent(mNativeVrShell, VrShell.this, true);
                    return true;
                } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    VrShellJni.get().onTriggerEvent(mNativeVrShell, VrShell.this, false);
                    return true;
                }
                return false;
            }
        };
    }

    public void initializeNative() {
        mNativeVrShell = VrShellJni.get().init(VrShell.this, mDelegate,
                getGvrApi().getNativeGvrContext(), mReprojectedRendering, mTab.getWebContents());

        mPresentationView.setOnTouchListener(mTouchListener);

        mContentVrWindowAndroid.setVSyncPaused(true);
        if (mOnVSyncPausedForTesting != null) {
            mOnVSyncPausedForTesting.run();
        }
    }

    private void reparentTab(WindowAndroid window) {
        mTab.updateAttachment(window, null);
    }

    // Exits VR, telling the user to remove their headset, and returning to Chromium.
    @CalledByNative
    public void forceExitVr() {
        mDelegate.shutdownVr(true, true);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        boolean parentConsumed = super.dispatchTouchEvent(event);
        if (mOnDispatchTouchEventForTesting != null) {
            mOnDispatchTouchEventForTesting.onDispatchTouchEvent(parentConsumed);
        }
        return parentConsumed;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mTab != null && mTab.getWebContents() != null
                && mTab.getWebContents().getEventForwarder().dispatchKeyEvent(event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (mTab != null && mTab.getWebContents() != null
                && mTab.getWebContents().getEventForwarder().onGenericMotionEvent(event)) {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void onResume() {
        if (mPaused != null && !mPaused) return;
        mPaused = false;
        super.onResume();
        if (mNativeVrShell != 0) {
            // Refreshing the viewer profile may accesses disk under some circumstances outside of
            // our control.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                VrShellJni.get().onResume(mNativeVrShell, VrShell.this);
            }
        }
    }

    @Override
    public void onPause() {
        if (mPaused != null && mPaused) return;
        mPaused = true;
        super.onPause();
        if (mNativeVrShell != 0) VrShellJni.get().onPause(mNativeVrShell, VrShell.this);
    }

    public void destroyWindowAndroid() {
        reparentTab(mWindowAndroid);
        mContentVrWindowAndroid.destroy();
    }

    @Override
    public void shutdown() {
        reparentTab(mWindowAndroid);
        if (mNativeVrShell != 0) {
            VrShellJni.get().destroy(mNativeVrShell, VrShell.this);
            mNativeVrShell = 0;
        }

        if (mTab != null) {
            TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.SHOWN, false);
        }

        mContentVrWindowAndroid.destroy();

        super.shutdown();
    }

    public void pause() {
        onPause();
    }

    public void resume() {
        onResume();
    }

    public void teardown() {
        shutdown();
    }

    public FrameLayout getContainer() {
        return this;
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (mNativeVrShell == 0) return;
        VrShellJni.get().setSurface(mNativeVrShell, VrShell.this, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // No need to do anything here, we don't care about surface width/height.
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        VrShellDelegate.forceExitVrImmediately();
    }

    @CalledByNative
    public boolean hasDaydreamSupport() {
        return VrCoreInstallUtils.hasDaydreamSupport();
    }

    /**
     * Sets the callback that will be run when VrShellImpl's dispatchTouchEvent
     * is run and the parent consumed the event.
     * @param callback The Callback to be run.
     */
    public void setOnDispatchTouchEventForTesting(OnDispatchTouchEventCallback callback) {
        mOnDispatchTouchEventForTesting = callback;
        ResettersForTesting.register(() -> mOnDispatchTouchEventForTesting = null);
    }

    /**
     * Sets that callback that will be run when VrShellImpl has issued the request to pause the
     * Android Window's VSyncs.
     * @param callback The Runnable to be run.
     */
    public void setOnVSyncPausedForTesting(Runnable callback) {
        mOnVSyncPausedForTesting = callback;
        ResettersForTesting.register(() -> mOnVSyncPausedForTesting = null);
    }

    public View getPresentationViewForTesting() {
        return mPresentationView;
    }

    @NativeMethods
    interface Natives {
        long init(VrShell caller, VrShellDelegate delegate, long gvrApi,
                boolean reprojectedRendering, WebContents webContents);
        void setSurface(long nativeVrShell, VrShell caller, Surface surface);
        void destroy(long nativeVrShell, VrShell caller);
        void onTriggerEvent(long nativeVrShell, VrShell caller, boolean touched);
        void onPause(long nativeVrShell, VrShell caller);
        void onResume(long nativeVrShell, VrShell caller);
    }
}
