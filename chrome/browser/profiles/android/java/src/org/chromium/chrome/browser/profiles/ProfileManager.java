// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ObserverList;

/**
 * Java interface to the C++ ProfileManager.
 */
public class ProfileManager {
    private static ObserverList<Observer> sObservers = new ObserverList<>();
    private static boolean sInitialized;

    /** Observer for Profile creation. */
    public static interface Observer {
        /**
         * Called whenever a profile is created.
         * @param profile The profile that has just been created.
         */
        public void onProfileAdded(Profile profile);

        /**
         * Called whenever a profile is destroyed.
         * @param profile The profile that has just been created.
         */
        public void onProfileDestroyed(Profile profile);
    }

    /**
     * Add an observer to be notified when profiles get created.
     */
    public static void addObserver(Observer observer) {
        sObservers.addObserver(observer);
    }

    /**
     * Remove an observer of profiles changes.
     */
    public static void removeObserver(Observer observer) {
        sObservers.removeObserver(observer);
    }

    /**
     * @return True iff any profile has been created.
     */
    public static boolean isInitialized() {
        return sInitialized;
    }

    @CalledByNative
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void onProfileAdded(Profile profile) {
        // If a profile has been added, we know the ProfileManager has been initialized.
        sInitialized = true;
        for (Observer observer : sObservers) {
            observer.onProfileAdded(profile);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void onProfileDestroyed(Profile profile) {
        for (Observer observer : sObservers) {
            observer.onProfileDestroyed(profile);
        }
    }

    public static void resetForTesting() {
        sInitialized = false;
        sObservers.clear();
    }
}
