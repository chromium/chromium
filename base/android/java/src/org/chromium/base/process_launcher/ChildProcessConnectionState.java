// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.build.annotations.NullMarked;

/**
 * The detailed service binding connection state.
 *
 * <p>This is for debugging/metrics purpose.
 */
@NullMarked
public final class ChildProcessConnectionState {
    // Whether the connection is bound with BIND_WAIVE_PRIORITY.
    public final boolean mIsWaivedBound;
    // Whether the connection is bound with BIND_NOT_PERCEPTIBLE.
    public final boolean mIsNotPerceptibleBound;
    // Whether the connection is bound with BIND_VISIBLE.
    public final boolean mIsVisibleBound;
    // Whether the connection is bound with BIND_STRONG.
    public final boolean mIsStrongBound;

    public ChildProcessConnectionState(
            boolean isWaivedBound,
            boolean isNotPerceptibleBound,
            boolean isVisibleBound,
            boolean isStrongBound) {
        mIsWaivedBound = isWaivedBound;
        mIsNotPerceptibleBound = isNotPerceptibleBound;
        mIsVisibleBound = isVisibleBound;
        mIsStrongBound = isStrongBound;
    }
}
