// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Function;

/**
 * A {@link ProfileKeyedMap} that can also provide an object before native is initialized, for the
 * profile that Chrome is expected to start with: the last used profile recorded by native, see
 * {@link ProfileManager#getLastUsedProfileBaseName()}.
 *
 * <p>Before native is initialized, {@link #getForProfile} takes a null profile and returns a single
 * pre-native instance. The expected profile is recorded when the pre-native instance is created.
 * The pre-native instance is moved to the expected profile when that profile or one of its
 * off-the-record profiles is first requested, or when it is the first regular profile added. If no
 * base name was recorded, or the first regular profile added is another profile, the pre-native
 * instance is destroyed, and the profile gets a new object the next time it is requested.
 *
 * <p>Once the first profile is added, callers must pass a profile, and should get the object again
 * with it instead of keeping using a pre-native instance they got before, since that instance might
 * have been destroyed.
 *
 * <p>All methods must be called on the UI thread.
 *
 * @param <T> The type of the objects in the map.
 */
@NullMarked
public class PreNativeProfileKeyedMap<T> {
    private final ProfileKeyedMap<T> mData;
    private final Function<@Nullable Profile, T> mFactory;
    private final @Nullable Callback<T> mDestroyAction;

    private @Nullable T mPreNativeInstance;
    private @Nullable String mPreNativeProfileBaseName;
    private ProfileManager.@Nullable Observer mProfileManagerObserver;

    /**
     * Creates a map that uses {@link ProfileKeyedMap.ProfileSelection#OWN_INSTANCE}.
     *
     * @param factory Constructs the object for a profile. It gets null when constructing the
     *     pre-native instance.
     * @param destroyAction The action to be taken on an object when its profile is destroyed, when
     *     this map is destroyed, or when the pre-native instance isn't for the profile Chrome
     *     started with.
     */
    public PreNativeProfileKeyedMap(
            Function<@Nullable Profile, T> factory, @Nullable Callback<T> destroyAction) {
        this(ProfileKeyedMap.ProfileSelection.OWN_INSTANCE, factory, destroyAction);
    }

    /**
     * @param profileSelection How off-the-record profiles are mapped to objects.
     * @param factory Constructs the object for a profile. It gets null when constructing the
     *     pre-native instance.
     * @param destroyAction The action to be taken on an object when its profile is destroyed, when
     *     this map is destroyed, or when the pre-native instance isn't for the profile Chrome
     *     started with.
     */
    public PreNativeProfileKeyedMap(
            @ProfileKeyedMap.ProfileSelection int profileSelection,
            Function<@Nullable Profile, T> factory,
            @Nullable Callback<T> destroyAction) {
        mFactory = factory;
        mDestroyAction = destroyAction;
        mData = new ProfileKeyedMap<>(profileSelection, destroyAction);
    }

    /**
     * Gets the object for a profile, creating it with the constructor's factory if needed. Unlike
     * {@link ProfileKeyedMap#getForProfile(Profile)}, this never returns null.
     *
     * @param profile The profile, or null before native is initialized for the profile that Chrome
     *     is expected to start with.
     * @return The object for the profile.
     * @throws IllegalStateException If the profile is null once the first profile is added.
     */
    public T getForProfile(@Nullable Profile profile) {
        ThreadUtils.assertOnUiThread();

        if (profile != null) {
            Profile originalProfile = profile.getOriginalProfile();
            if (isProfileMatched(originalProfile)) {
                movePreNativeInstanceToProfileMap(originalProfile);
            }
            // May return different instances for a regular profile and its off-the-record
            // profiles, depending on the ProfileSelection of this map.
            return mData.getForProfile(profile, nonNullProfile -> mFactory.apply(nonNullProfile));
        }

        if (ProfileManager.isInitialized()) {
            throw new IllegalStateException("Pass a Profile once the first profile is added");
        }

        if (mPreNativeInstance == null) {
            mPreNativeInstance = mFactory.apply(null);
            mPreNativeProfileBaseName = ProfileManager.getLastUsedProfileBaseName();
            mProfileManagerObserver =
                    new ProfileManager.Observer() {
                        @Override
                        public void onProfileAdded(Profile profile) {
                            onFirstProfileAdded(profile);
                        }

                        @Override
                        public void onProfileDestroyed(Profile profile) {
                            // No profile can be destroyed before the first one is added. Objects
                            // moved to a profile are cleaned up by `mData`.
                        }
                    };
            ProfileManager.addObserver(mProfileManagerObserver);
        }
        return mPreNativeInstance;
    }

    /** Destroys this map, and all the objects in it. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        destroyInstance(takePreNativeInstance());
        mData.destroy();
    }

    private void onFirstProfileAdded(Profile profile) {
        // Defensive: ProfileManager only notifies observers about regular profiles, and an
        // off-the-record profile has the same base name as its original profile.
        if (profile.isOffTheRecord()) return;

        if (isProfileMatched(profile)) {
            movePreNativeInstanceToProfileMap(profile);
            return;
        }

        // No base name was recorded, or Chrome started with another profile, e.g. because the
        // base name was stale, so the pre-native instance may have been set up for the wrong
        // profile.
        destroyInstance(takePreNativeInstance());
    }

    private boolean isProfileMatched(Profile profile) {
        return mPreNativeInstance != null
                && profile.getBaseName().equals(mPreNativeProfileBaseName);
    }

    private void movePreNativeInstanceToProfileMap(Profile profile) {
        assert !profile.isOffTheRecord();
        // Requesting an object for the profile moves the pre-native instance first.
        assert mData.getForProfile(profile) == null;
        T preNativeInstance = assumeNonNull(takePreNativeInstance());
        mData.getForProfile(profile, unused -> preNativeInstance);
        // TODO(crbug.com/591111726): Notify the pre-native instance that it is moved to its
        // profile, once a user needs it.
    }

    /** Stops tracking the pre-native instance, and returns it. */
    private @Nullable T takePreNativeInstance() {
        if (mProfileManagerObserver != null) {
            ProfileManager.removeObserver(mProfileManagerObserver);
            mProfileManagerObserver = null;
        }
        T preNativeInstance = mPreNativeInstance;
        mPreNativeInstance = null;
        mPreNativeProfileBaseName = null;
        return preNativeInstance;
    }

    private void destroyInstance(@Nullable T instance) {
        if (instance == null || mDestroyAction == null) return;

        mDestroyAction.onResult(instance);
    }

    /** Returns a map that destroys its {@link Destroyable} objects when they are discarded. */
    public static <T extends Destroyable>
            PreNativeProfileKeyedMap<T> createMapOfDestroyablesForTesting(
                    Function<@Nullable Profile, T> factory) {
        return new PreNativeProfileKeyedMap<>(factory, Destroyable::destroy);
    }
}
