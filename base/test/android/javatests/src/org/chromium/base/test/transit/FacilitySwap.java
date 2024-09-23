// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.List;

/** A {@link Transition} out of one or more {@link Facility}s into another {@link Facility}. */
public class FacilitySwap extends Transition {
    private List<Facility<?>> mFacilitiesToExit;
    private Facility<?> mFacilityToEnter;

    /**
     * Constructor. FacilitySwap is instantiated to move between Facilities.
     *
     * @param facilitiesToExit the {@link Facility}s to exit.
     * @param facilityToEnter the {@link Facility} to enter.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition. e.g. clicking a View.
     */
    public <F extends Facility<?>> FacilitySwap(
            List<Facility<?>> facilitiesToExit,
            F facilityToEnter,
            TransitionOptions options,
            Trigger trigger) {
        super(options, facilitiesToExit, List.of(facilityToEnter), trigger);
        assert !facilitiesToExit.isEmpty();
        mFacilitiesToExit = facilitiesToExit;
        mFacilityToEnter = facilityToEnter;
    }

    @Override
    public String toDebugString() {
        String facilitiesToExitString;
        if (mFacilitiesToExit.size() > 1) {
            facilitiesToExitString = mFacilitiesToExit.toString();
        } else {
            facilitiesToExitString = mFacilitiesToExit.get(0).toString();
        }
        return String.format(
                "FacilitySwap %d (from %s to %s)", mId, facilitiesToExitString, mFacilityToEnter);
    }
}
