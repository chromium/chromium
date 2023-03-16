// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;

import java.util.HashMap;
import java.util.Map;

/**
 * A Profile-lifetime aware data structure that allows mapping objects to Profile and handling the
 * necessary cleanup when a Profile is destroyed.
 *
 * This data structure owns the objects associated with the Profile and will delete them when
 * appropriate.
 *
 * @param <T> The type of object being mapped to the Profile.
 */
public class ProfileKeyedMap<T> {
    /** Indicates no cleanup action is required when destroying an object in the map. */
    public static final Callback NO_REQUIRED_CLEANUP_ACTION = null;

    private final Map<Profile, T> mData = new HashMap<>();
    private final @Nullable Callback<T> mDestroyAction;

    private ProfileManager.Observer mProfileManagerObserver;

    /**
     * Creates a map of Profile -> Object that handles automatically cleaning up when the profiles
     * are destroyed.
     * @param destroyAction The action to be taken on the object during destruction of this map
     *                      or when a Profile is destroyed.
     */
    public ProfileKeyedMap(@Nullable Callback<T> destroyAction) {
        mDestroyAction = destroyAction;
    }

    /**
     * @return A data structure that maps Profile to Destroyable objects that will be destroyed
     *         when appropriate.
     * @param <T> The object type being mapped against the Profile.
     */
    public static <T extends Destroyable> ProfileKeyedMap<T> createMapOfDestroyables() {
        return new ProfileKeyedMap<>((e) -> e.destroy());
    }

    /**
     * Gets (and lazily constructs if needed) the mapped object  for a given Profile.
     * @param profile The Profile the object is associated with.
     * @param factory The factory that will construct the object if it does not already exist.
     * @return The object associated with the passed in Profile.
     */
    public T getForProfile(Profile profile, Supplier<T> factory) {
        T obj = mData.get(profile);
        if (obj == null) {
            obj = factory.get();
            mData.put(profile, obj);
        }
        if (mProfileManagerObserver == null) {
            mProfileManagerObserver = new ProfileManager.Observer() {
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

    /**
     * Destroys this object and all objects currently mapped to Profiles.
     */
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
}
