// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

import java.util.List;

/** Java interface to the C++ ProfileManager. */
public class ProfileManager {
    private static Profile sLastUsedProfileForTesting;

    private static ObserverList<Observer> sObservers;
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

    /** Add an observer to be notified when profiles get created. */
    public static void addObserver(Observer observer) {
        // Lazily creating the ObserverList to avoid the internal ThreadChecker being assigned to
        // the first thread that accesses any ProfileManager method. Because the ObserverList
        // asserts thread consistency, this does not do additional locking on the ProfileManager
        // side because the internal ObserverList checks will crash if thread misuse happens.
        if (sObservers == null) {
            sObservers = new ObserverList<>();
        }
        sObservers.addObserver(observer);
    }

    /** Remove an observer of profiles changes. */
    public static void removeObserver(Observer observer) {
        if (sObservers == null) return;
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

        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onProfileAdded(profile);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void onProfileDestroyed(Profile profile) {
        if (sObservers == null) return;
        for (Observer observer : sObservers) {
            observer.onProfileDestroyed(profile);
        }
    }

    /**
     * Returns the regular (i.e., not off-the-record) profile.
     *
     * <p>Note: The function name uses the "last used" terminology for consistency with
     * profile_manager.cc which supports multiple regular profiles.
     */
    public static Profile getLastUsedRegularProfile() {
        if (sLastUsedProfileForTesting != null) {
            return sLastUsedProfileForTesting;
        }
        ThreadUtils.assertOnUiThread();
        if (!ProfileManager.isInitialized()) {
            throw new IllegalStateException("Browser hasn't finished initialization yet!");
        }
        return (Profile) ProfileManagerJni.get().getLastUsedRegularProfile();
    }

    /** Return the fully loaded and initialized Profiles (excluding off the record Profiles). */
    public static List<Profile> getLoadedProfiles() {
        return ProfileManagerJni.get().getLoadedProfiles();
    }

    public static void onProfileActivated(Profile profile) {
        ProfileManagerJni.get().onProfileActivated(profile);
    }

    /**
     * Destroys the Profile. Destruction is delayed until all associated renderers have been killed,
     * so the profile might not be destroyed upon returning from this call.
     */
    public static void destroyWhenAppropriate(Profile profile) {
        profile.ensureNativeInitialized();
        ProfileManagerJni.get().destroyWhenAppropriate(profile);
    }

    /** Sets for testing the profile to be returned by {@link #getLastUsedRegularProfile()}. */
    public static void setLastUsedProfileForTesting(Profile profile) {
        sLastUsedProfileForTesting = profile;
        ResettersForTesting.register(() -> sLastUsedProfileForTesting = null);
    }

    public static void resetForTesting() {
        sInitialized = false;
        if (sObservers != null) sObservers.clear();
    }

    @NativeMethods
    public interface Natives {
        Object getLastUsedRegularProfile();

        void onProfileActivated(@JniType("Profile*") Profile profile);

        void destroyWhenAppropriate(@JniType("Profile*") Profile caller);

        @JniType("std::vector<Profile*>")
        List<Profile> getLoadedProfiles();
    }
}
