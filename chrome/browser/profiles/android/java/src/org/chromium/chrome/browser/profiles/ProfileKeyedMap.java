// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

/**
 * A Profile-lifetime aware data structure that allows mapping objects to Profile and handling the
 * necessary cleanup when a Profile is destroyed.
 *
 * <p>This data structure owns the objects associated with the Profile and will delete them when
 * appropriate.
 *
 * @param <T> The type of object being mapped to the Profile.
 */
public class ProfileKeyedMap<T> {
    /** Indicates no cleanup action is required when destroying an object in the map. */
    public static final Callback NO_REQUIRED_CLEANUP_ACTION = null;

    /** Uses to determine what Profile reference should be used and stored in the map. */
    @IntDef({ProfileSelection.OWN_INSTANCE, ProfileSelection.REDIRECTED_TO_ORIGINAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ProfileSelection {
        /** Original: Self -- OTR: Self */
        int OWN_INSTANCE = 0;

        /** Original: Self -- OTR: Original */
        int REDIRECTED_TO_ORIGINAL = 1;
    }

    private final Map<Profile, T> mData = new HashMap<>();
    @ProfileSelection private final int mProfileSelection;
    private final @Nullable Callback<T> mDestroyAction;

    private ProfileManager.Observer mProfileManagerObserver;

    /**
     * Creates a map of Profile -> Object that handles automatically cleaning up when the profiles
     * are destroyed. Utilizes {@link ProfileSelection#OWN_INSTANCE} for determining the profile.
     *
     * @param destroyAction The action to be taken on the object during destruction of this map or
     *     when a Profile is destroyed.
     */
    public ProfileKeyedMap(@Nullable Callback<T> destroyAction) {
        this(ProfileSelection.OWN_INSTANCE, destroyAction);
    }

    /**
     * Creates a map of Profile -> Object that handles automatically cleaning up when the profiles
     * are destroyed.
     *
     * @param profileSelection Determines what {@link Profile} should be used.
     * @param destroyAction The action to be taken on the object during destruction of this map or
     *     when a Profile is destroyed.
     */
    public ProfileKeyedMap(
            @ProfileSelection int profileSelection, @Nullable Callback<T> destroyAction) {
        mProfileSelection = profileSelection;
        mDestroyAction = destroyAction;
    }

    /**
     * @return A data structure that maps Profile to Destroyable objects that will be destroyed when
     *     appropriate. Utilizes {@link ProfileSelection#OWN_INSTANCE} for determining the profile.
     * @param <T> The object type being mapped against the Profile.
     */
    public static <T extends Destroyable> ProfileKeyedMap<T> createMapOfDestroyables() {
        return createMapOfDestroyables(ProfileSelection.OWN_INSTANCE);
    }

    /**
     * @param profileSelection Determines what {@link Profile} should be used.
     * @return A data structure that maps Profile to Destroyable objects that will be destroyed when
     *     appropriate.
     * @param <T> The object type being mapped against the Profile.
     */
    public static <T extends Destroyable> ProfileKeyedMap<T> createMapOfDestroyables(
            @ProfileSelection int profileSelection) {
        return new ProfileKeyedMap<>(profileSelection, (e) -> e.destroy());
    }

    private static Profile getProfileToUse(
            Profile profile, @ProfileSelection int profileSelection) {
        if (profileSelection == ProfileSelection.REDIRECTED_TO_ORIGINAL) {
            return profile.getOriginalProfile();
        }
        assert profileSelection == ProfileSelection.OWN_INSTANCE;
        return profile;
    }

    /**
     * Gets (and lazily constructs if needed) the mapped object for a given Profile.
     *
     * @param profile The Profile the object is associated with.
     * @param factory The factory that will construct the object if it does not already exist.
     * @return The object associated with the passed in Profile.
     */
    public T getForProfile(Profile profile, Function<Profile, T> factory) {
        profile = getProfileToUse(profile, mProfileSelection);

        // TODO(crbug.com/365814339): Convert to checked exception once all call sites are fixed.
        assert !profile.shutdownStarted()
                : "Attempting to access profile keyed data on destroyed Profile";

        T obj = mData.get(profile);
        if (obj == null) {
            obj = factory.apply(profile);
            mData.put(profile, obj);
        }
        if (mProfileManagerObserver == null) {
            mProfileManagerObserver =
                    new ProfileManager.Observer() {
                        @Override
                        public void onProfileAdded(Profile profile) {}

                        @Override
                        public void onProfileDestroyed(Profile destroyedProfile) {
                            T obj = mData.remove(destroyedProfile);
                            if (obj == null) return;
                            if (mDestroyAction != null) mDestroyAction.onResult(obj);
                        }
                    };
            ProfileManager.addObserver(mProfileManagerObserver);
        }
        return obj;
    }

    /** Destroys this object and all objects currently mapped to Profiles. */
    public void destroy() {
        if (mProfileManagerObserver != null) ProfileManager.removeObserver(mProfileManagerObserver);
        mProfileManagerObserver = null;
        if (mDestroyAction != null) {
            for (var obj : mData.values()) {
                mDestroyAction.onResult(obj);
            }
        }
        mData.clear();
    }

    /** @return The number of Profile -> obj mappings that exist. */
    public int size() {
        return mData.size();
    }

    /**
     * Return the list of {@link Profile}s that have be used in a successful call to {@link
     * #getForProfile(Profile, Function)} on this map.
     */
    public List<Profile> getTrackedProfiles() {
        return new ArrayList<>(mData.keySet());
    }
}
