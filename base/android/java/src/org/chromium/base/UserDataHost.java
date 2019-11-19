// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Process;

import java.util.HashMap;

/**
 * A class that implements type-safe heterogeneous container. It can associate
 * an object of type T with a type token (T.class) as a key. Mismatch of the
 * type between them can be checked at compile time, hence type-safe. Objects
 * are held using strong reference in the container. {@code null} is not allowed
 * for key or object.
 * <p>
 * Can be used for an object that needs to have other objects attached to it
 * without having to manage explicit references to them. Attached objects need
 * to implement {@link UserData} so that they can be destroyed by {@link #destroy()}.
 * <p>
 * No operation takes effect once {@link #destroy()} is called.
 * <p>
 * Usage:

 *
 * <code>
 * public class Foo {
 *     // Defines the container.
 *     private final UserDataHost mUserDataHost = new UserDataHost();
 *
 *     public UserDataHost getUserDataHost() {
 *         return mUserDataHost;
 *     }
 * }
 *
 * public class FooBar implements UserData {
 *
 *     public FooBar from(UserDataHost host) {
 *         FooBar foobar = host.getUserData(FooBar.class);
 *         // Instantiate FooBar upon the first access.
 *         return foobar != null ? foobar : host.setUserData(FooBar.class, new FooBar());
 *     }
 * }
 *
 *     Foo foo = new Foo();
 *     ...
 *
 *     FooBar bar = FooBar.from(foo.getUserDataHost());
 *
 *     ...
 *
 * </code>
 */
public final class UserDataHost {
    private final long mThreadId = Process.myTid();

    private HashMap<Class<? extends UserData>, UserData> mUserDataMap = new HashMap<>();

    private static void checkArgument(boolean condition) {
        if (!condition) {
            throw new IllegalArgumentException(
                    "Neither key nor object of UserDataHost can be null.");
        }
    }

    private void checkThreadAndState() {
        if (mThreadId != Process.myTid()) {
            throw new IllegalStateException("UserData must only be used on a single thread.");
        }
        if (mUserDataMap == null) {
            throw new IllegalStateException("Operation is not allowed after destroy().");
        }
    }

    /**
     * Associates the specified object with the specified key.
     * @param key Type token with which the specified object is to be associated.
     * @param object Object to be associated with the specified key.
     * @return the object just stored, or {@code null} if storing the object failed.
     */
    public <T extends UserData> T setUserData(Class<T> key, T object) {
        checkThreadAndState();
        checkArgument(key != null && object != null);

        mUserDataMap.put(key, object);
        return getUserData(key);
    }

    /**
     * Returns the value to which the specified key is mapped, or null if this map
     * contains no mapping for the key.
     * @param key Type token for which the specified object is to be returned.
     * @return the value to which the specified key is mapped, or null if this map
     *         contains no mapping for {@code key}.
     */
    public <T extends UserData> T getUserData(Class<T> key) {
        checkThreadAndState();
        checkArgument(key != null);

        return key.cast(mUserDataMap.get(key));
    }

    /**
     * Removes the mapping for a key from this map. Exception will be thrown if
     * the given key has no mapping.
     * @param key Type token for which the specified object is to be removed.
     * @return The previous value associated with {@code key}.
     */
    public <T extends UserData> T removeUserData(Class<T> key) {
        checkThreadAndState();
        checkArgument(key != null);

        if (!mUserDataMap.containsKey(key)) {
            throw new IllegalStateException("UserData for the key is not present.");
        }
        return key.cast(mUserDataMap.remove(key));
    }

    /**
     * Destroy all the managed {@link UserData} instances. This should be invoked at
     * the end of the lifetime of the host that user data instances hang on to.
     * The host stops managing them after this method is called.
     */
    public void destroy() {
        checkThreadAndState();

        // Nulls out |mUserDataMap| first in order to prevent concurrent modification that
        // might happen in the for loop below.
        HashMap<Class<? extends UserData>, UserData> map = mUserDataMap;
        mUserDataMap = null;
        for (UserData userData : map.values()) userData.destroy();
    }
}
