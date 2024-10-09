// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifetime;

import org.jni_zero.CalledByNative;

import org.chromium.base.ObserverList;

/** Watches for when Chrome is told to restart itself. */
public class ApplicationLifetime {
    /** Interface to be implemented to be notified of application termination. */
    public interface Observer {
        /**
         * Called when the application should be terminated.
         * @param restart Whether or not to restart Chrome.
         */
        void onTerminate(boolean restart);
    }

    private static ObserverList<Observer> sObservers = new ObserverList<Observer>();

    /**
     * Adds an observer to watch for application termination.
     * @param observer The observer to add.
     */
    public static void addObserver(Observer observer) {
        sObservers.addObserver(observer);
    }

    /**
     * Removes an observer from watching for application termination.
     *
     * @param observer The observer to remove.
     */
    public static void removeObserver(Observer observer) {
        sObservers.removeObserver(observer);
    }

    @CalledByNative
    public static void terminate(boolean restart) {
        for (Observer observer : sObservers) {
            observer.onTerminate(restart);
        }
    }
}
