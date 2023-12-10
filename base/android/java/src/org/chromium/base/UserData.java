// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/**
 * Interface to be implemented by the classes make themselves attacheable to
 * a host class that holds {@link UserDataHost}.
 */
public interface UserData {
    /**
     * Called when {@link UserData} object needs to be destroyed.
     *
     * <pre>
     * WARNING: This method is not guaranteed to be called. Each host class should
     *          call {@link UserDataHost#destroy()} explicitly at the end of its
     *          lifetime to have all of its {@link UserData#destroy()} get invoked.
     * </pre>
     */
    default void destroy() {}
}
