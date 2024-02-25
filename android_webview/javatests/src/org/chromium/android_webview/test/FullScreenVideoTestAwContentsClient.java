// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** This class is a AwContentsClient for full screen video test. */
public class FullScreenVideoTestAwContentsClient extends TestAwContentsClient {
    public static final long WAITING_SECONDS = 20L;
    private CallbackHelper mOnShowCustomViewCallbackHelper = new CallbackHelper();
    private CallbackHelper mOnHideCustomViewCallbackHelper = new CallbackHelper();
    private CallbackHelper mOnUnhandledKeyUpEventCallbackHelper = new CallbackHelper();

    private Runnable mOnHideCustomViewRunnable;
    private final Activity mActivity;
    private final boolean mAllowHardwareAcceleration;
    private View mCustomView;
    private AwContentsClient.CustomViewCallback mExitCallback;

    public FullScreenVideoTestAwContentsClient(
            Activity activity, boolean allowHardwareAcceleration) {
        mActivity = activity;
        mAllowHardwareAcceleration = allowHardwareAcceleration;
    }

    @Override
    public void onShowCustomView(View view, AwContentsClient.CustomViewCallback callback) {
        mCustomView = view;
        if (!mAllowHardwareAcceleration) {
            // The hardware emulation in the testing infrastructure is not perfect, and this is
            // required to work-around some of the limitations.
            mCustomView.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        }
        mExitCallback = callback;
        mActivity
                .getWindow()
                .setFlags(
                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                        WindowManager.LayoutParams.FLAG_FULLSCREEN);

        mActivity
                .getWindow()
                .addContentView(
                        view,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                Gravity.CENTER));
        mOnShowCustomViewCallbackHelper.notifyCalled();
    }

    /** Sets a task that will be run when {@link #onHideCustomView()} is invoked. */
    public void setOnHideCustomViewRunnable(Runnable runnable) {
        mOnHideCustomViewRunnable = runnable;
    }

    @Override
    public void onHideCustomView() {
        mActivity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        mOnHideCustomViewCallbackHelper.notifyCalled();
        if (mOnHideCustomViewRunnable != null) {
            mOnHideCustomViewRunnable.run();
        }
    }

    public AwContentsClient.CustomViewCallback getExitCallback() {
        return mExitCallback;
    }

    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_UP) {
            mOnUnhandledKeyUpEventCallbackHelper.notifyCalled();
        }
    }

    public boolean wasOnUnhandledKeyUpEventCalled() {
        return mOnUnhandledKeyUpEventCallbackHelper.getCallCount() > 0;
    }

    public View getCustomView() {
        return mCustomView;
    }

    public boolean wasCustomViewShownCalled() {
        return mOnShowCustomViewCallbackHelper.getCallCount() > 0;
    }

    public void waitForCustomViewShown() throws TimeoutException {
        mOnShowCustomViewCallbackHelper.waitForCallback(0, 1, WAITING_SECONDS, TimeUnit.SECONDS);
    }

    public void waitForCustomViewHidden() throws TimeoutException {
        mOnHideCustomViewCallbackHelper.waitForCallback(0, 1, WAITING_SECONDS, TimeUnit.SECONDS);
    }
}
