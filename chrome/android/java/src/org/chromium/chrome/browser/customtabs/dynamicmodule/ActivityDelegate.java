// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.Bundle;
import android.os.RemoteException;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;

import javax.inject.Inject;

/**
 * A wrapper around a {@link IActivityDelegate}.
 *
 * No {@link RemoteException} should ever be thrown as all of this code runs in the same process.
 */
public class ActivityDelegate implements StartStopWithNativeObserver,
        PauseResumeWithNativeObserver, Destroyable,
        SaveInstanceStateObserver, WindowFocusChangedObserver {
    private IActivityDelegate mActivityDelegate;

    private boolean mModuleOnStartPending;
    private boolean mModuleOnResumePending;
    private ChromeActivity mActivity;

    private Bundle getSavedInstanceState() {
        return mActivity.getSavedInstanceState();
    }

    @Inject
    public ActivityDelegate(
            ChromeActivity chromeActivity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher) {

        mActivity = chromeActivity;
        activityLifecycleDispatcher.register(this);
    }

    public void setActivityDelegate(IActivityDelegate activityDelegate) {
        mActivityDelegate = activityDelegate;

        safeRun(() -> mActivityDelegate.onCreate(getSavedInstanceState()));

        if (mModuleOnStartPending) startModule();
        if (mModuleOnResumePending) resumeModule();
    }

    private void startModule() {
        assert mActivityDelegate != null;

        mModuleOnStartPending = false;
        safeRun(() -> {
            mActivityDelegate.onStart();
            mActivityDelegate.onRestoreInstanceState(getSavedInstanceState());
            mActivityDelegate.onPostCreate(getSavedInstanceState());
        });
    }

    private void resumeModule() {
        assert mActivityDelegate != null;

        mModuleOnResumePending = false;
        safeRun(mActivityDelegate::onResume);
    }

    @Override
    public void destroy() {
        if (mActivityDelegate != null) {
            safeRun(mActivityDelegate::onDestroy);
            mActivityDelegate = null;
        }
    }

    @Override
    public void onStartWithNative() {
        if (mActivityDelegate != null) {
            startModule();
        } else {
            mModuleOnStartPending = true;
        }
    }

    @Override
    public void onStopWithNative() {
        if (mActivityDelegate != null) {
            safeRun(mActivityDelegate::onStop);
        }
        mModuleOnStartPending = false;
    }

    @Override
    public void onResumeWithNative() {
        if (mActivityDelegate != null) {
            resumeModule();
        } else {
            mModuleOnResumePending = true;
        }
    }

    @Override
    public void onPauseWithNative() {
        if (mActivityDelegate != null) {
            safeRun(mActivityDelegate::onPause);
        }
        mModuleOnResumePending = false;
    }


    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (mActivityDelegate != null) {
            safeRun(() -> mActivityDelegate.onSaveInstanceState(outState));
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (mActivityDelegate != null) {
            safeRun(() -> mActivityDelegate.onWindowFocusChanged(hasFocus));
        }
    }

    public void onBackPressedAsync(Runnable notHandledRunnable) {
        if (mActivityDelegate != null) {
            safeRun(() -> mActivityDelegate.onBackPressedAsync(
                    ObjectWrapper.wrap(notHandledRunnable)));
        }
    }

    @VisibleForTesting
    public IActivityDelegate getActivityDelegateForTesting() {
        return mActivityDelegate;
    }

    private interface RemoteRunnable { void run() throws RemoteException; }

    private interface RemoteCallable<T> { T call() throws RemoteException; }

    private void safeRun(RemoteRunnable runnable) {
        try {
            runnable.run();
        } catch (RemoteException e) {
            assert false;
        }
    }

    private <T> T safeCall(RemoteCallable<T> callable, T defaultReturn) {
        try {
            return callable.call();
        } catch (RemoteException e) {
            assert false;
        }

        return defaultReturn;
    }

    public void onRestoreInstanceState(Bundle savedInstanceState) {
        safeRun(() -> mActivityDelegate.onRestoreInstanceState(savedInstanceState));
    }

    public void onNavigationEvent(int navigationEvent, Bundle extras) {
        safeRun(() -> mActivityDelegate.onNavigationEvent(navigationEvent, extras));
    }

    public void onPageMetricEvent(String metricName, long navigationStart,
            long offset, long navigationId) {
        safeRun(() -> mActivityDelegate.onPageMetricEvent(
                metricName, navigationStart, offset, navigationId));
    }

    public void onMessageChannelReady() {
        safeRun(mActivityDelegate::onMessageChannelReady);
    }

    public void onPostMessage(String message) {
        safeRun(() -> mActivityDelegate.onPostMessage(message));
    }
}
