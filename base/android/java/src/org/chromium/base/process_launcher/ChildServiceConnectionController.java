// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Intent;

import org.chromium.base.ChildBindingState;
import org.chromium.build.annotations.NullMarked;

/**
 * Manages the bindings of a {@link ChildProcessConnection}. This class is not threadsafe and should
 * only be used on the launcher thread.
 *
 * <p>This is an abstraction to allow different binding strategies, like using multiple parallel
 * bindings on older Android versions vs using Context.rebindService() on newer versions.
 *
 * <p>The {@link #bind(int)} method should be called to initially bind the connection. After that,
 * the binding level can be adjusted by calling {@link #addStrongBinding()}, {@link
 * #addVisibleBinding()}, {@link #addNotPerceptibleBinding()} and their `remove` counterparts. The
 * controller does not count the number of calls to these methods, so the caller is responsible for
 * ensuring that `add...Binding()` is not called multiple times for the same binding level before
 * the corresponding `remove...Binding()` is called.
 */
@NullMarked
/* package */ interface ChildServiceConnectionController {
    /**
     * Binds the connection.
     *
     * @param initialBindingState The binding state to use initially.
     * @return true if the binding succeeds.
     */
    boolean bind(@ChildBindingState int initialBindingState);

    /** Unbinds the connection. */
    void unbind();

    /** Rebinds the service. */
    void rebind();

    /**
     * Calls `Context.updateServiceGroup()` if possible.
     *
     * @return {@code true} if the call succeeds.
     */
    boolean updateGroupImportance(int group, int importanceInGroup);

    /**
     * Recreates the connection with a new {@link Intent}.
     *
     * <p>This is used to recreate the connection with a different service name.
     *
     * <p>This retires all existing bindings and reproduces them with the new intent.
     */
    void replaceService(Intent bindIntent);

    /**
     * Sets the effective binding state of the connection.
     *
     * <p>This is available only if EffectiveBindingState feature is enabled. This method will
     * deprecate the usage of set/unsetting strong/visible/not perceptible bindings below.
     *
     * @param effectiveBindingState The effective binding state to set.
     */
    void setEffectiveBindingState(@ChildBindingState int effectiveBindingState);

    /**
     * Adds a strong binding to the service, making it a foreground priority process.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void setStrongBinding();

    /**
     * Removes a strong binding.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void unsetStrongBinding();

    /**
     * Adds a visible binding to the service.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void setVisibleBinding();

    /**
     * Removes a visible binding.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void unsetVisibleBinding();

    /**
     * Adds a "not perceptible" binding.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void setNotPerceptibleBinding();

    /**
     * Removes a "not perceptible" binding.
     *
     * <p>TODO(crbug.com/444561927): Remove this method once the EffectiveBindingState feature is
     * enabled.
     */
    void unsetNotPerceptibleBinding();

    /**
     * @return the current connection binding state. This method is threadsafe.
     */
    @ChildBindingState
    int getBindingState();

    /**
     * @return the binding state when the connection died, or current state if alive. This method is
     *     threadsafe.
     */
    @ChildBindingState
    int getBindingStateCurrentOrWhenDied();

    /**
     * @return true if the connection is unbound.
     */
    boolean isUnbound();

    /**
     * @return the detailed binding state for debugging purposes.
     */
    ChildProcessConnectionState getConnectionStateForDebugging();
}
