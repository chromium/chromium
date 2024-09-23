// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityResultWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.OnUserLeaveHintObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.RecreateObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;

/**
 * Dispatches lifecycle events of activities extending {@link AsyncInitializationActivity} to
 * registered observers.
 *
 * All observers will be automatically cleared when the backing activity is destroyed.
 */
public class ActivityLifecycleDispatcherImpl implements ActivityLifecycleDispatcher {
    private final ObserverList<InflationObserver> mInflationObservers = new ObserverList<>();
    private final ObserverList<NativeInitObserver> mNativeInitObservers = new ObserverList<>();
    private final ObserverList<PauseResumeWithNativeObserver> mPauseResumeObservers =
            new ObserverList<>();
    private final ObserverList<StartStopWithNativeObserver> mStartStopObservers =
            new ObserverList<>();
    private final ObserverList<DestroyObserver> mDestroyables = new ObserverList<>();
    private final ObserverList<SaveInstanceStateObserver> mSaveInstanceStateObservers =
            new ObserverList<>();
    private final ObserverList<WindowFocusChangedObserver> mWindowFocusChangesObservers =
            new ObserverList<>();
    private final ObserverList<ActivityResultWithNativeObserver>
            mActivityResultWithNativeObservers = new ObserverList<>();
    private final ObserverList<ConfigurationChangedObserver> mConfigurationChangedListeners =
            new ObserverList<>();
    private final ObserverList<RecreateObserver> mRecreateObservers = new ObserverList<>();
    private final ObserverList<OnUserLeaveHintObserver> mOnUserLeaveHintObservers =
            new ObserverList<>();
    private final ObserverList<TopResumedActivityChangedObserver>
            mTopResumedActivityChangedObservers = new ObserverList<>();

    private final Activity mActivity;

    private @ActivityState int mActivityState = ActivityState.DESTROYED;
    private boolean mIsNativeInitialized;
    private boolean mDestroyed;

    public ActivityLifecycleDispatcherImpl(Activity activity) {
        mActivity = activity;
    }

    @Override
    public void register(LifecycleObserver observer) {
        if (observer instanceof InflationObserver) {
            mInflationObservers.addObserver((InflationObserver) observer);
        }
        if (observer instanceof PauseResumeWithNativeObserver) {
            mPauseResumeObservers.addObserver((PauseResumeWithNativeObserver) observer);
        }
        if (observer instanceof StartStopWithNativeObserver) {
            mStartStopObservers.addObserver((StartStopWithNativeObserver) observer);
        }
        if (observer instanceof NativeInitObserver) {
            mNativeInitObservers.addObserver((NativeInitObserver) observer);
        }
        if (observer instanceof DestroyObserver) {
            mDestroyables.addObserver((DestroyObserver) observer);
        }
        if (observer instanceof SaveInstanceStateObserver) {
            mSaveInstanceStateObservers.addObserver((SaveInstanceStateObserver) observer);
        }
        if (observer instanceof WindowFocusChangedObserver) {
            mWindowFocusChangesObservers.addObserver((WindowFocusChangedObserver) observer);
        }
        if (observer instanceof ActivityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.addObserver(
                    (ActivityResultWithNativeObserver) observer);
        }
        if (observer instanceof ConfigurationChangedObserver) {
            mConfigurationChangedListeners.addObserver((ConfigurationChangedObserver) observer);
        }
        if (observer instanceof RecreateObserver) {
            mRecreateObservers.addObserver((RecreateObserver) observer);
        }
        if (observer instanceof OnUserLeaveHintObserver) {
            mOnUserLeaveHintObservers.addObserver((OnUserLeaveHintObserver) observer);
        }
        if (observer instanceof TopResumedActivityChangedObserver) {
            mTopResumedActivityChangedObservers.addObserver(
                    (TopResumedActivityChangedObserver) observer);
        }
    }

    @Override
    public void unregister(LifecycleObserver observer) {
        if (observer instanceof InflationObserver) {
            mInflationObservers.removeObserver((InflationObserver) observer);
        }
        if (observer instanceof PauseResumeWithNativeObserver) {
            mPauseResumeObservers.removeObserver((PauseResumeWithNativeObserver) observer);
        }
        if (observer instanceof StartStopWithNativeObserver) {
            mStartStopObservers.removeObserver((StartStopWithNativeObserver) observer);
        }
        if (observer instanceof NativeInitObserver) {
            mNativeInitObservers.removeObserver((NativeInitObserver) observer);
        }
        if (observer instanceof DestroyObserver) {
            mDestroyables.removeObserver((DestroyObserver) observer);
        }
        if (observer instanceof SaveInstanceStateObserver) {
            mSaveInstanceStateObservers.removeObserver((SaveInstanceStateObserver) observer);
        }
        if (observer instanceof WindowFocusChangedObserver) {
            mWindowFocusChangesObservers.removeObserver((WindowFocusChangedObserver) observer);
        }
        if (observer instanceof ActivityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.removeObserver(
                    (ActivityResultWithNativeObserver) observer);
        }
        if (observer instanceof ConfigurationChangedObserver) {
            mConfigurationChangedListeners.removeObserver((ConfigurationChangedObserver) observer);
        }
        if (observer instanceof RecreateObserver) {
            mRecreateObservers.removeObserver((RecreateObserver) observer);
        }
        if (observer instanceof OnUserLeaveHintObserver) {
            mOnUserLeaveHintObservers.removeObserver((OnUserLeaveHintObserver) observer);
        }
        if (observer instanceof TopResumedActivityChangedObserver) {
            mTopResumedActivityChangedObservers.removeObserver(
                    (TopResumedActivityChangedObserver) observer);
        }
    }

    @Override
    public int getCurrentActivityState() {
        return mActivityState;
    }

    @Override
    public boolean isNativeInitializationFinished() {
        return mIsNativeInitialized;
    }

    @Override
    public boolean isActivityFinishingOrDestroyed() {
        return mDestroyed || mActivity.isFinishing();
    }

    void dispatchPreInflationStartup() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onPreInflationStartup();
        }
    }

    void dispatchOnInflationComplete() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onInflationComplete();
        }
    }

    void dispatchPostInflationStartup() {
        if (isActivityFinishingOrDestroyed()) return;
        for (InflationObserver observer : mInflationObservers) {
            observer.onPostInflationStartup();
        }
    }

    void onCreateWithNative() {
        mActivityState = ActivityState.CREATED_WITH_NATIVE;
    }

    void dispatchOnResumeWithNative() {
        mActivityState = ActivityState.RESUMED_WITH_NATIVE;
        for (PauseResumeWithNativeObserver observer : mPauseResumeObservers) {
            observer.onResumeWithNative();
        }
    }

    void dispatchOnPauseWithNative() {
        mActivityState = ActivityState.PAUSED_WITH_NATIVE;
        for (PauseResumeWithNativeObserver observer : mPauseResumeObservers) {
            observer.onPauseWithNative();
        }
    }

    void dispatchOnStartWithNative() {
        mActivityState = ActivityState.STARTED_WITH_NATIVE;
        for (StartStopWithNativeObserver observer : mStartStopObservers) {
            observer.onStartWithNative();
        }
    }

    void dispatchOnStopWithNative() {
        mActivityState = ActivityState.STOPPED_WITH_NATIVE;
        for (StartStopWithNativeObserver observer : mStartStopObservers) {
            observer.onStopWithNative();
        }
    }

    void dispatchNativeInitializationFinished() {
        mIsNativeInitialized = true;
        for (NativeInitObserver observer : mNativeInitObservers) {
            observer.onFinishNativeInitialization();
        }
    }

    void onDestroyStarted() {
        mDestroyed = true;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    protected void dispatchOnDestroy() {
        mActivityState = ActivityState.DESTROYED;

        for (DestroyObserver destroyable : mDestroyables) {
            destroyable.onDestroy();
        }

        // Drain observers to prevent possible memory leaks.
        // TODO(twellington): Add some state to this class to prevent observers from being
        //                    registered after the activity has been destroyed.
        mInflationObservers.clear();
        mPauseResumeObservers.clear();
        mStartStopObservers.clear();
        mNativeInitObservers.clear();
        mSaveInstanceStateObservers.clear();
        mWindowFocusChangesObservers.clear();
        mActivityResultWithNativeObservers.clear();
        mConfigurationChangedListeners.clear();
        mDestroyables.clear();
        mRecreateObservers.clear();
        mTopResumedActivityChangedObservers.clear();
    }

    void dispatchOnSaveInstanceState(Bundle outBundle) {
        for (SaveInstanceStateObserver observer : mSaveInstanceStateObservers) {
            observer.onSaveInstanceState(outBundle);
        }
    }

    void dispatchOnWindowFocusChanged(boolean hasFocus) {
        for (WindowFocusChangedObserver observer : mWindowFocusChangesObservers) {
            observer.onWindowFocusChanged(hasFocus);
        }
    }

    void dispatchOnActivityResultWithNative(int requestCode, int resultCode, Intent data) {
        for (ActivityResultWithNativeObserver observer : mActivityResultWithNativeObservers) {
            observer.onActivityResultWithNative(requestCode, resultCode, data);
        }
    }

    void dispatchOnConfigurationChanged(Configuration newConfig) {
        for (ConfigurationChangedObserver observer : mConfigurationChangedListeners) {
            observer.onConfigurationChanged(newConfig);
        }
    }

    void dispatchOnRecreate() {
        for (RecreateObserver observer : mRecreateObservers) {
            observer.onRecreate();
        }
    }

    void dispatchOnUserLeaveHint() {
        for (OnUserLeaveHintObserver observer : mOnUserLeaveHintObservers) {
            observer.onUserLeaveHint();
        }
    }

    void dispatchOnTopResumedActivityChanged(boolean isTopResumedActivity) {
        for (TopResumedActivityChangedObserver observer : mTopResumedActivityChangedObservers) {
            observer.onTopResumedActivityChanged(isTopResumedActivity);
        }
    }
}
