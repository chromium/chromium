// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;

/**
 * Listener to be informed whenever they have been detached from the host. This
 * can happen when the particular {@link UnownedUserDataHost} they are attached
 * to is destroyed.
 */
@NullMarked
public interface UnownedUserDataListener<T> {
    /**
     * Invoked whenever the particular UnownedUserData has been removed from a particular host. If
     * the UnownedUserData has been garbage collected before the UserDataHost is informed of its
     * removal, this method will of course not be invoked.
     *
     * <p>This method is invoked asynchronously, but from the correct thread.
     *
     * @param host from which host the UnownedUserData was detached.
     */
    void onDetachedFromHost(T value, UnownedUserDataHost host);
}
