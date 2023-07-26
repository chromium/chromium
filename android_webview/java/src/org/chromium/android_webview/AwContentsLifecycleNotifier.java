// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * This class is intended to notify observers of the existence native instances of
 * aw_contents. It receives a callback when native aw_contents are created or
 * destroyed. Observers are notified when the first instance is created or the
 * last instance is destroyed. This tracks all WebViews across all profiles.
 */
@JNINamespace("android_webview")
@Lifetime.Singleton
public class AwContentsLifecycleNotifier {
    /**
     * Observer interface to be implemented by deriving webview lifecycle observers.
     */
    public static interface Observer {
        public void onFirstWebViewCreated();
        public void onLastWebViewDestroyed();
    }

    private static final ObserverList<Observer> sLifecycleObservers =
            new ObserverList<Observer>();
    private static boolean sHasWebViewInstances;
    private static @AppState int sAppState = AppState.DESTROYED;

    private AwContentsLifecycleNotifier() {}

    public static void addObserver(Observer observer) {
        sLifecycleObservers.addObserver(observer);
    }

    public static void removeObserver(Observer observer) {
        sLifecycleObservers.removeObserver(observer);
    }

    public static boolean hasWebViewInstances() {
        return sHasWebViewInstances;
    }

    public static @AppState int getAppState() {
        return sAppState;
    }

    // Called on UI thread.
    @CalledByNative
    private static void onFirstWebViewCreated() {
        ThreadUtils.assertOnUiThread();
        sHasWebViewInstances = true;
        // first webview created, notify observers.
        for (Observer observer : sLifecycleObservers) {
            observer.onFirstWebViewCreated();
        }
    }

    // Called on UI thread.
    @CalledByNative
    private static void onLastWebViewDestroyed() {
        ThreadUtils.assertOnUiThread();
        sHasWebViewInstances = false;
        // last webview destroyed, notify observers.
        for (Observer observer : sLifecycleObservers) {
            observer.onLastWebViewDestroyed();
        }
    }

    // Called on UI thread.
    @CalledByNative
    private static void onAppStateChanged(@AppState int appState) {
        ThreadUtils.assertOnUiThread();
        sAppState = appState;
    }
}
