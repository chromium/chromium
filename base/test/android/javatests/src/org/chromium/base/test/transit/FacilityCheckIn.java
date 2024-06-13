// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.Collections;
import java.util.List;

/** A {@link Transition} into a {@link Facility}. */
class FacilityCheckIn extends Transition {
    private final Facility mFacility;

    /**
     * Constructor. FacilityCheckIn is instantiated to enter a {@link Facility}.
     *
     * @param facility the {@link Facility} to enter.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition into the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckIn(Facility facility, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, Collections.EMPTY_LIST, List.of(facility), trigger);
        mFacility = facility;
    }

    @Override
    public String toDebugString() {
        return String.format("FacilityCheckIn %d (enter %s)", mId, mFacility);
    }
}
