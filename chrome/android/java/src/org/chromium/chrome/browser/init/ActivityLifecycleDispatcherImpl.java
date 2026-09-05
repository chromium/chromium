// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;
import android.os.PersistableBundle;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.WindowFocusChangedObserver;

/**
 * Dispatches lifecycle events of activities extending {@link AsyncInitializationActivity} to
 * registered observers.
 *
 * <p>All observers will be automatically cleared when the backing activity is destroyed.
 */
@NullMarked
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
    private final ObserverList<TopResumedActivityChangedWithNativeObserver>
            mTopResumedActivityChangedWithNativeObservers = new ObserverList<>();

    private @Nullable Activity mActivity;

    private @ActivityState int mActivityState = ActivityState.DESTROYED;
    private boolean mIsNativeInitialized;
    private boolean mDestroyed;

    public ActivityLifecycleDispatcherImpl(@Nullable Activity activity) {
        mActivity = activity;
    }

    @Override
    public void register(LifecycleObserver observer) {
        if (mActivity == null) {
            return;
        }
        if (observer instanceof InflationObserver inflationObserver) {
            mInflationObservers.addObserver(inflationObserver);
        }
        if (observer instanceof PauseResumeWithNativeObserver pauseResumeWithNativeObserver) {
            mPauseResumeObservers.addObserver(pauseResumeWithNativeObserver);
        }
        if (observer instanceof StartStopWithNativeObserver startStopWithNativeObserver) {
            mStartStopObservers.addObserver(startStopWithNativeObserver);
        }
        if (observer instanceof NativeInitObserver nativeInitObserver) {
            mNativeInitObservers.addObserver(nativeInitObserver);
        }
        if (observer instanceof DestroyObserver destroyObserver) {
            mDestroyables.addObserver(destroyObserver);
        }
        if (observer instanceof SaveInstanceStateObserver saveInstanceStateObserver) {
            mSaveInstanceStateObservers.addObserver(saveInstanceStateObserver);
        }
        if (observer instanceof WindowFocusChangedObserver windowFocusChangedObserver) {
            mWindowFocusChangesObservers.addObserver(windowFocusChangedObserver);
        }
        if (observer instanceof ActivityResultWithNativeObserver activityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.addObserver(activityResultWithNativeObserver);
        }
        if (observer instanceof ConfigurationChangedObserver configurationChangedObserver) {
            mConfigurationChangedListeners.addObserver(configurationChangedObserver);
        }
        if (observer instanceof RecreateObserver recreateObserver) {
            mRecreateObservers.addObserver(recreateObserver);
        }
        if (observer instanceof OnUserLeaveHintObserver onUserLeaveHintObserver) {
            mOnUserLeaveHintObservers.addObserver(onUserLeaveHintObserver);
        }
        if (observer
                instanceof TopResumedActivityChangedObserver topResumedActivityChangedObserver) {
            mTopResumedActivityChangedObservers.addObserver(topResumedActivityChangedObserver);
        }
        if (observer
                instanceof
                TopResumedActivityChangedWithNativeObserver
                        topResumedActivityChangedWithNativeObserver) {
            mTopResumedActivityChangedWithNativeObservers.addObserver(
                    topResumedActivityChangedWithNativeObserver);
        }
    }

    @Override
    public void unregister(LifecycleObserver observer) {
        if (observer instanceof InflationObserver inflationObserver) {
            mInflationObservers.removeObserver(inflationObserver);
        }
        if (observer instanceof PauseResumeWithNativeObserver pauseResumeWithNativeObserver) {
            mPauseResumeObservers.removeObserver(pauseResumeWithNativeObserver);
        }
        if (observer instanceof StartStopWithNativeObserver startStopWithNativeObserver) {
            mStartStopObservers.removeObserver(startStopWithNativeObserver);
        }
        if (observer instanceof NativeInitObserver nativeInitObserver) {
            mNativeInitObservers.removeObserver(nativeInitObserver);
        }
        if (observer instanceof DestroyObserver destroyObserver) {
            mDestroyables.removeObserver(destroyObserver);
        }
        if (observer instanceof SaveInstanceStateObserver saveInstanceStateObserver) {
            mSaveInstanceStateObservers.removeObserver(saveInstanceStateObserver);
        }
        if (observer instanceof WindowFocusChangedObserver windowFocusChangedObserver) {
            mWindowFocusChangesObservers.removeObserver(windowFocusChangedObserver);
        }
        if (observer instanceof ActivityResultWithNativeObserver activityResultWithNativeObserver) {
            mActivityResultWithNativeObservers.removeObserver(activityResultWithNativeObserver);
        }
        if (observer instanceof ConfigurationChangedObserver configurationChangedObserver) {
            mConfigurationChangedListeners.removeObserver(configurationChangedObserver);
        }
        if (observer instanceof RecreateObserver recreateObserver) {
            mRecreateObservers.removeObserver(recreateObserver);
        }
        if (observer instanceof OnUserLeaveHintObserver onUserLeaveHintObserver) {
            mOnUserLeaveHintObservers.removeObserver(onUserLeaveHintObserver);
        }
        if (observer
                instanceof TopResumedActivityChangedObserver topResumedActivityChangedObserver) {
            mTopResumedActivityChangedObservers.removeObserver(topResumedActivityChangedObserver);
        }
        if (observer
                instanceof
                TopResumedActivityChangedWithNativeObserver
                        topResumedActivityChangedWithNativeObserver) {
            mTopResumedActivityChangedWithNativeObservers.removeObserver(
                    topResumedActivityChangedWithNativeObserver);
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
        return mDestroyed || mActivity == null || mActivity.isFinishing();
    }

    void dispatchPreInflationStartup() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onPreInflationStartup();
        }
    }

    void dispatchOnInflationComplete() {
        if (isActivityFinishingOrDestroyed()) return;
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

    public void dispatchOnDestroy() {
        mDestroyed = true;
        mActivityState = ActivityState.DESTROYED;

        // Clear mActivity to prevent future calls to register().
        mActivity = null;

        for (DestroyObserver destroyable : mDestroyables) {
            destroyable.onDestroy();
        }

        // Drain observers to prevent possible memory leaks.
        mInflationObservers.clear();
        mNativeInitObservers.clear();
        mPauseResumeObservers.clear();
        mStartStopObservers.clear();
        mDestroyables.clear();
        mSaveInstanceStateObservers.clear();
        mWindowFocusChangesObservers.clear();
        mActivityResultWithNativeObservers.clear();
        mConfigurationChangedListeners.clear();
        mRecreateObservers.clear();
        mOnUserLeaveHintObservers.clear();
        mTopResumedActivityChangedObservers.clear();
        mTopResumedActivityChangedWithNativeObservers.clear();
    }

    void dispatchOnSaveInstanceState(Bundle outBundle) {
        for (SaveInstanceStateObserver observer : mSaveInstanceStateObservers) {
            observer.onSaveInstanceState(outBundle);
        }
    }

    void dispatchOnSaveInstanceState(Bundle outBundle, PersistableBundle outPersistentState) {
        for (SaveInstanceStateObserver observer : mSaveInstanceStateObservers) {
            observer.onSaveInstanceState(outBundle, outPersistentState);
        }
    }

    void dispatchOnWindowFocusChanged(boolean hasFocus) {
        for (WindowFocusChangedObserver observer : mWindowFocusChangesObservers) {
            observer.onWindowFocusChanged(hasFocus);
        }
    }

    void dispatchOnActivityResultWithNative(
            int requestCode, int resultCode, @Nullable Intent data) {
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

    void dispatchOnTopResumedActivityChangedWithNative(boolean isTopResumedActivity) {
        for (TopResumedActivityChangedWithNativeObserver observer :
                mTopResumedActivityChangedWithNativeObservers) {
            observer.onTopResumedActivityChangedWithNative(isTopResumedActivity);
        }
    }
}
