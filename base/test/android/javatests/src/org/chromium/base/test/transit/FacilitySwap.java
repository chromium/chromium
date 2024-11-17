// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.List;

/** A {@link Transition} out of one or more {@link Facility}s into another {@link Facility}. */
public class FacilitySwap extends Transition {
    private final List<Facility<?>> mFacilitiesToExit;
    private final List<Facility<?>> mFacilitiesToEnter;

    /**
     * Constructor. FacilitySwap is instantiated to move between Facilities.
     *
     * @param facilitiesToExit the {@link Facility}s to exit.
     * @param facilitiesToEnter the {@link Facility} to enter.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition. e.g. clicking a View.
     */
    public FacilitySwap(
            List<Facility<?>> facilitiesToExit,
            List<Facility<?>> facilitiesToEnter,
            TransitionOptions options,
            Trigger trigger) {
        super(options, facilitiesToExit, facilitiesToEnter, trigger);
        assert !facilitiesToExit.isEmpty();
        mFacilitiesToExit = facilitiesToExit;
        mFacilitiesToEnter = facilitiesToEnter;
    }

    @Override
    public String toDebugString() {
        String facilitiesToExitString;
        String facilitiesToEnterString;
        if (mFacilitiesToExit.size() > 1) {
            facilitiesToExitString = mFacilitiesToExit.toString();
        } else {
            facilitiesToExitString = mFacilitiesToExit.get(0).toString();
        }
        if (mFacilitiesToEnter.size() > 1) {
            facilitiesToEnterString = mFacilitiesToEnter.toString();
        } else {
            facilitiesToEnterString = mFacilitiesToEnter.get(0).toString();
        }
        return String.format(
                "FacilitySwap %d (from %s to %s)",
                mId, facilitiesToExitString, facilitiesToEnterString);
    }
}
