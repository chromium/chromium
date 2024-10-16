// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Wrapper that allows passing a Profile reference around in the Java layer. */
public class Profile implements BrowserContextHandle {
    private final @Nullable OtrProfileId mOtrProfileId;

    /** Pointer to the Native-side Profile. */
    private long mNativeProfile;

    private boolean mDestroyNotified;

    @CalledByNative
    private Profile(long nativeProfile, @Nullable OtrProfileId otrProfileId) {
        mNativeProfile = nativeProfile;
        mOtrProfileId = otrProfileId;
    }

    /**
     * @param webContents {@link WebContents} object.
     * @return {@link Profile} object associated with the given WebContents.
     */
    public static Profile fromWebContents(WebContents webContents) {
        return ProfileJni.get().fromWebContents(webContents);
    }

    /**
     * Handles type conversion of Java side {@link BrowserContextHandle} to {@link Profile}.
     *
     * @param browserContextHandle Java reference to native BrowserContext.
     * @return A strongly typed reference the {@link Profile}.
     */
    public static Profile fromBrowserContextHandle(BrowserContextHandle browserContextHandle) {
        return (Profile) browserContextHandle;
    }

    /**
     * Returns the {@link BrowserProfileType} for the corresponding profile.
     *
     * Please note {@link BrowserProfileType} is generated from native so, it also contains other
     * types of Profile like Guest and System that we don't support in Android.
     */
    public static @BrowserProfileType int getBrowserProfileTypeFromProfile(Profile profile) {
        assert profile != null;

        if (!profile.isOffTheRecord()) return BrowserProfileType.REGULAR;
        if (profile.isPrimaryOtrProfile()) return BrowserProfileType.INCOGNITO;
        return BrowserProfileType.OTHER_OFF_THE_RECORD_PROFILE;
    }

    public Profile getOriginalProfile() {
        return ProfileJni.get().getOriginalProfile(mNativeProfile);
    }

    /** Return whether this Profile represents the initially created "Default" Profile. */
    public boolean isInitialProfile() {
        return ProfileJni.get().isInitialProfile(mNativeProfile);
    }

    /**
     * Returns the OffTheRecord profile with given OtrProfileiD. If the profile does not exist and
     * createIfNeeded is true, a new profile is created, otherwise returns null.
     *
     * @param profileId {@link OtrProfileId} object.
     * @param createIfNeeded Boolean indicating the profile should be created if doesn't exist.
     */
    public Profile getOffTheRecordProfile(OtrProfileId profileId, boolean createIfNeeded) {
        assert profileId != null;
        return ProfileJni.get().getOffTheRecordProfile(mNativeProfile, profileId, createIfNeeded);
    }

    /**
     * Returns the OffTheRecord profile for incognito tabs. If the profile does not exist and
     * createIfNeeded is true, a new profile is created, otherwise returns null.
     *
     * @param createIfNeeded Boolean indicating the profile should be created if doesn't exist.
     */
    public Profile getPrimaryOtrProfile(boolean createIfNeeded) {
        return ProfileJni.get().getPrimaryOtrProfile(mNativeProfile, createIfNeeded);
    }

    /**
     * Returns the OffTheRecord profile id for OffTheRecord profiles, and null for regular profiles.
     */
    public @Nullable OtrProfileId getOtrProfileId() {
        return mOtrProfileId;
    }

    /**
     * Returns if OffTheRecord profile with given OtrProfileId exists.
     *
     * @param profileId {@link OtrProfileId} object.
     */
    public boolean hasOffTheRecordProfile(OtrProfileId profileId) {
        assert profileId != null;
        return ProfileJni.get().hasOffTheRecordProfile(mNativeProfile, profileId);
    }

    /** Returns if primary OffTheRecord profile exists. */
    public boolean hasPrimaryOtrProfile() {
        return ProfileJni.get().hasPrimaryOtrProfile(mNativeProfile);
    }

    /** Returns if the profile is a primary OTR Profile. */
    public boolean isPrimaryOtrProfile() {
        return mOtrProfileId != null && mOtrProfileId.isPrimaryOtrId();
    }

    /**
     * Returns if the profile is a primary OTR Profile or an Incognito CCT. The primary OTR profile
     * is the OffTheRecord profile for incognito tabs in the main Chrome App. All Incognito branded
     * profiles return true for {@link #isOffTheRecord()} but not all OffTheRecord profiles are
     * Incognito themed. Use this to evaluate features that should appear exclusively for Incognito
     * themed profiles (Incognito lock, Incognito snapshot controller..) or for usages that force
     * the Incognito theme (Dark colors, Incognito logo..). If you are unsure whether this fits your
     * usage, reach out to incognito/OWNERS.
     */
    public boolean isIncognitoBranded() {
        boolean isIncognitoCCT = mOtrProfileId != null && mOtrProfileId.isIncognitoCCId();
        return isPrimaryOtrProfile() || isIncognitoCCT;
    }

    /**
     * Returns if the profile is off the record. Off the record sessions are not persistent and
     * browsing data generated within this profile is cleared after the session ends. Note that this
     * does not imply Incognito as other OTR sessions (e.g. Ephemeral CCT) are not Incognito
     * branded.
     */
    public boolean isOffTheRecord() {
        return mOtrProfileId != null;
    }

    public ProfileKey getProfileKey() {
        return ProfileJni.get().getProfileKey(mNativeProfile);
    }

    /**
     * @return Whether the profile is signed in to a child account.
     * @deprecated Please use {@link
     *     org.chromium.components.signin.base.AccountCapabilities#isSubjectToParentalControls}
     *     instead.
     */
    @Deprecated
    public boolean isChild() {
        return ProfileJni.get().isChild(mNativeProfile);
    }

    /** Wipes all data for this profile. */
    public void wipe() {
        ProfileJni.get().wipe(mNativeProfile);
    }

    /**
     * @return Whether or not the native side profile exists.
     */
    @VisibleForTesting
    public boolean isNativeInitialized() {
        return mNativeProfile != 0;
    }

    /**
     * When called, raises an exception if the native pointer is not initialized. This is useful to
     * get a more debuggable stacktrace than failing on native-side when dereferencing.
     */
    public void ensureNativeInitialized() {
        if (mNativeProfile == 0) {
            throw new RuntimeException("Native profile pointer not initialized.");
        }
    }

    @Override
    public long getNativeBrowserContextPointer() {
        return mNativeProfile;
    }

    /**
     * Returns whether shutdown has been initiated. This is a signal that the object will be
     * destroyed soon and no new references to this object should be created.
     */
    public boolean shutdownStarted() {
        return mDestroyNotified;
    }

    private void notifyWillBeDestroyed() {
        assert !mDestroyNotified;
        mDestroyNotified = true;

        ProfileManager.onProfileDestroyed(this);
    }

    @CalledByNative
    private void onProfileWillBeDestroyed() {
        notifyWillBeDestroyed();
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeProfile = 0;

        if (!mDestroyNotified) {
            assert false : "Destroy should have been notified previously.";
            notifyWillBeDestroyed();
        }
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeProfile;
    }

    @NativeMethods
    public interface Natives {
        Profile fromWebContents(WebContents webContents);

        Profile getOriginalProfile(long ptr);

        boolean isInitialProfile(long ptr);

        Profile getOffTheRecordProfile(long ptr, OtrProfileId otrProfileId, boolean createIfNeeded);

        Profile getPrimaryOtrProfile(long ptr, boolean createIfNeeded);

        boolean hasOffTheRecordProfile(long ptr, OtrProfileId otrProfileId);

        boolean hasPrimaryOtrProfile(long ptr);

        boolean isChild(long ptr);

        void wipe(long ptr);

        ProfileKey getProfileKey(long ptr);
    }
}
