// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A {@link Transition} into a {@link Facility}. */
class FacilityCheckIn extends Transition {
    private final String mFacilityNames;

    /**
     * Constructor. FacilityCheckIn is instantiated to enter a {@link Facility}.
     *
     * @param facilities the {@link Facility}s to enter.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition into the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckIn(
            List<Facility<?>> facilities, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, Collections.EMPTY_LIST, facilities, trigger);

        List<String> names = new ArrayList<>();
        for (Facility<?> facility : facilities) {
            names.add(facility.getName());
        }
        mFacilityNames = String.join(", ", names);
    }

    @Override
    public String toDebugString() {
        return String.format("FacilityCheckIn %d (enter %s)", mId, mFacilityNames);
    }
}
