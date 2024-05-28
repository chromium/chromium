// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;

import java.util.Collections;
import java.util.List;

/** A {@link Transition} out of a {@link Facility}. */
class FacilityCheckOut extends Transition {
    private Facility mFacility;

    /**
     * Constructor. FacilityCheckOut is instantiated to leave a {@link Facility}.
     *
     * @param facility the {@link Facility} to leave.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition out of the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckOut(Facility facility, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, List.of(facility), Collections.EMPTY_LIST, trigger);
        mFacility = facility;
    }

    @Override
    public String toDebugString() {
        return String.format("FacilityCheckOut %d (exit %s)", mId, mFacility);
    }

    @Override
    protected List<ConditionWait> createWaits() {
        return calculateConditionWaits(
                mFacility.getElements(), Elements.EMPTY, getTransitionConditions());
    }
}
