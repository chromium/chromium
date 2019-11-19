// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.content.res.Configuration;
import android.os.Bundle;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityResultWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
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
    private final ObserverList<Destroyable> mDestroyables = new ObserverList<>();
    private final ObserverList<SaveInstanceStateObserver> mSaveInstanceStateObservers =
            new ObserverList<>();
    private final ObserverList<WindowFocusChangedObserver> mWindowFocusChangesObservers =
            new ObserverList<>();
    private final ObserverList<ActivityResultWithNativeObserver>
            mActivityResultWithNativeObservers = new ObserverList<>();
    private final ObserverList<ConfigurationChangedObserver> mConfigurationChangedListeners =
            new ObserverList<>();

    private @ActivityState int mActivityState = ActivityState.DESTROYED;

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
        if (observer instanceof Destroyable) {
            mDestroyables.addObserver((Destroyable) observer);
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
        if (observer instanceof Destroyable) {
            mDestroyables.removeObserver((Destroyable) observer);
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
    }

    @Override
    public int getCurrentActivityState() {
        return mActivityState;
    }

    void dispatchPreInflationStartup() {
        for (InflationObserver observer : mInflationObservers) {
            observer.onPreInflationStartup();
        }
    }

    void dispatchPostInflationStartup() {
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
        for (NativeInitObserver observer : mNativeInitObservers) {
            observer.onFinishNativeInitialization();
        }
    }

    void dispatchOnDestroy() {
        mActivityState = ActivityState.DESTROYED;

        for (Destroyable destroyable : mDestroyables) {
            destroyable.destroy();
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
}
