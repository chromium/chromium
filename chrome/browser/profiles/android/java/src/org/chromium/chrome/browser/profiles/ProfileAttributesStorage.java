// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.ArrayList;
import java.util.List;

/**
 * Java counterpart of the C++ {@code ProfileAttributesStorage}, which includes the profiles that
 * aren't loaded. Created by native before Java is notified of the first profile.
 */
@NullMarked
public class ProfileAttributesStorage {
    /** Observer of the profiles, mirroring C++ {@code ProfileAttributesStorageObserver}. */
    public interface Observer {
        default void onProfileAdded(String profilePath) {}

        default void onProfileWasRemoved(String profilePath) {}

        default void onProfileAuthInfoChanged(String profilePath) {}

        default void onProfileIsOmittedChanged(String profilePath) {}
    }

    private static @Nullable ProfileAttributesStorage sInstance;
    private static @Nullable ProfileAttributesStorage sInstanceForTesting;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    ProfileAttributesStorage() {}

    /** Returns the storage. Must only be called after native has created it. */
    public static ProfileAttributesStorage get() {
        ThreadUtils.assertOnUiThread();
        if (sInstanceForTesting != null) return sInstanceForTesting;
        assert sInstance != null : "Native hasn't created the ProfileAttributesStorage yet.";
        return sInstance;
    }

    public static void setInstanceForTesting(ProfileAttributesStorage storage) {
        sInstanceForTesting = storage;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /** Returns one entry per known profile, in no particular order. */
    List<ProfileAttributesEntry> getAllProfilesAttributes() {
        return ProfileAttributesStorageJni.get().getAllProfilesAttributes();
    }

    /**
     * Returns the account IDs of the primary accounts of all the profiles, skipping signed-out and
     * omitted profiles.
     */
    public List<CoreAccountId> getPrimaryAccountIdsForAllProfiles() {
        List<CoreAccountId> accountIds = new ArrayList<>();
        for (ProfileAttributesEntry entry : getAllProfilesAttributes()) {
            if (entry.isOmitted()) continue;
            GaiaId gaiaId = entry.getGaiaId();
            if (gaiaId != null) accountIds.add(new CoreAccountId(gaiaId));
        }
        return accountIds;
    }

    /** Returns whether the account is the primary account of a profile that isn't omitted. */
    public boolean isPrimaryAccountForAnyProfile(CoreAccountId accountId) {
        return getPrimaryAccountIdsForAllProfiles().contains(accountId);
    }

    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @CalledByNative
    private static ProfileAttributesStorage create() {
        sInstance = new ProfileAttributesStorage();
        return sInstance;
    }

    @CalledByNative
    private void destroy() {
        sInstance = null;
    }

    @CalledByNative
    void onProfileAdded(@JniType("std::string") String profilePath) {
        for (Observer observer : mObservers) observer.onProfileAdded(profilePath);
    }

    @CalledByNative
    void onProfileWasRemoved(@JniType("std::string") String profilePath) {
        for (Observer observer : mObservers) observer.onProfileWasRemoved(profilePath);
    }

    @CalledByNative
    void onProfileAuthInfoChanged(@JniType("std::string") String profilePath) {
        for (Observer observer : mObservers) observer.onProfileAuthInfoChanged(profilePath);
    }

    @CalledByNative
    void onProfileIsOmittedChanged(@JniType("std::string") String profilePath) {
        for (Observer observer : mObservers) observer.onProfileIsOmittedChanged(profilePath);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::vector<jni_zero::ScopedJavaLocalRef<jobject>>")
        List<ProfileAttributesEntry> getAllProfilesAttributes();
    }
}
