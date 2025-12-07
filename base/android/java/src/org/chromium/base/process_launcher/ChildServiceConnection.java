// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Interface representing a connection to the Android service. Can be mocked in unit-tests. */
@NullMarked
/* package */ interface ChildServiceConnection {
    boolean bindServiceConnection();

    /**
     * Unbinds the underlying service connection.
     *
     * @param onUnbindCallback A callback to be run after isBound() become false and before the
     *     service is unbound.
     */
    void unbindServiceConnection(@Nullable Runnable onStateChangeCallback);

    boolean isBound();

    /**
     * Calls `Context.updateServiceGroup()` if possible.
     *
     * <p>Returns `true` if the call succeeds.
     *
     * <p>Note that we need to rebind a service binding for the process to apply the change of this.
     */
    boolean updateGroupImportance(int group, int importanceInGroup);

    void retire();

    void rebindService(int bindFlags);
}
