// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Build;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.metrics.TrackExitReasonsOfInterest;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;

/**
 * This class is intended to notify observers of the existence native instances of
 * aw_contents. It receives a callback when native aw_contents are created or
 * destroyed. Observers are notified when the first instance is created or the
 * last instance is destroyed. This tracks all WebViews across all profiles.
 */
@JNINamespace("android_webview")
@Lifetime.Singleton
public class AwContentsLifecycleNotifier {
    // Initializing here means that the class will not initialize the LazyHolder members
    // until #getInstance is called since LazyHolder is a separate class.
    // This should prevent the members from being initialized anywhere, but the UIThread since
    // all calls to addObserver and removeObserver depend on getInstance and are called from
    // the UIThread.
    private static class LazyHolder {
        static final AwContentsLifecycleNotifier sInstance = new AwContentsLifecycleNotifier();
    }

    /** @return the singleton AwContentsLifecycleNotifier. */
    @CalledByNative
    public static AwContentsLifecycleNotifier getInstance() {
        ThreadUtils.assertOnUiThread();
        return LazyHolder.sInstance;
    }

    /** Observer interface to be implemented by deriving webview lifecycle observers. */
    public static interface Observer {
        public void onFirstWebViewCreated();

        public void onLastWebViewDestroyed();
    }

    private boolean mHasWebViewInstances;
    private volatile @AppState int mAppState = AppState.DESTROYED;

    private final ObserverList<Observer> mLifecycleObservers = new ObserverList<Observer>();

    private AwContentsLifecycleNotifier() {}

    public void addObserver(Observer observer) {
        mLifecycleObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mLifecycleObservers.removeObserver(observer);
    }

    public boolean hasWebViewInstances() {
        return mHasWebViewInstances;
    }

    // Calls to this are thread safe
    public @AppState int getAppState() {
        return mAppState;
    }

    // Called on UI thread.
    @CalledByNative
    private void onFirstWebViewCreated() {
        ThreadUtils.assertOnUiThread();
        mHasWebViewInstances = true;
        // first webview created, notify observers.
        for (Observer observer : mLifecycleObservers) {
            observer.onFirstWebViewCreated();
        }
    }

    // Called on UI thread.
    @CalledByNative
    private void onLastWebViewDestroyed() {
        ThreadUtils.assertOnUiThread();
        mHasWebViewInstances = false;
        // last webview destroyed, notify observers.
        for (Observer observer : mLifecycleObservers) {
            observer.onLastWebViewDestroyed();
        }
    }

    // Called on UI thread.
    @CalledByNative
    private void onAppStateChanged(@AppState int appState) {
        ThreadUtils.assertOnUiThread();
        mAppState = appState;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            TrackExitReasonsOfInterest.writeLastWebViewState();
        }
    }

    @CalledByNative
    public static void initialize() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            TrackExitReasonsOfInterest.init(AwContentsLifecycleNotifier.getInstance()::getAppState);
        }
    }
}
