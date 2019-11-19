// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;

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
        public void onProfileCreated(Profile profile);
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
    private static void onProfileAdded(Profile profile) {
        // If a profile has been added, we know the ProfileManager has been initialized.
        sInitialized = true;
        for (Observer observer : sObservers) {
            observer.onProfileCreated(profile);
        }
    }
}
