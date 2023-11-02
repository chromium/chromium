// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Marker interface to be implemented by classes which makes them attachable to a host class that
 * holds {@link UnownedUserDataHost} entries.
 * <p>
 * Marking something as a UnownedUserData has no other implications than that the class can be
 * referenced from a {@link UnownedUserDataHost} as a {@link java.lang.ref.WeakReference}.
 * <p>
 * Implementors can also optionally implement the method
 * {@link #onDetachedFromHost(UnownedUserDataHost)}
 * to be informed whenever they have been detached from the host. This can happen when the
 * particular {@link UnownedUserDataHost} they are attached to is destroyed.
 *
 * @see UnownedUserDataHost for more details on ownership and typical usage.
 * @see UnownedUserDataKey for information about the type of key that is required.
 */
public interface UnownedUserData {
    /**
     * Invoked whenever the particular UnownedUserData has been removed from a particular host. If
     * the UnownedUserData has been garbage collected before the UserDataHost is informed of its
     * removal, this method will of course not be invoked.
     * <p>
     * This method is invoked asynchronously, but from the correct thread.
     *
     * @param host from which host the UnownedUserData was detached.
     */
    default void onDetachedFromHost(UnownedUserDataHost host) {}

    /**
     * WARNING: This may be invoked in a re-entrant way, but will be invoked on the correct thread.
     *
     * @return true if the UnownedUserData wants to be informed asynchronously about detachments.
     */
    default boolean informOnDetachmentFromHost() {
        return true;
    }
}
