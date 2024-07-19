// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A {@link Transition} out of a {@link Facility}. */
class FacilityCheckOut extends Transition {
    private final String mFacilityNames;

    /**
     * Constructor. FacilityCheckOut is instantiated to leave one or more {@link Facility}s.
     *
     * @param facilities the {@link Facility}s to leave.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition out of the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckOut(
            List<Facility<?>> facilities, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, facilities, Collections.emptyList(), trigger);

        List<String> names = new ArrayList<>();
        for (Facility<?> facility : facilities) {
            names.add(facility.getName());
        }
        mFacilityNames = String.join(", ", names);
    }

    @Override
    public String toDebugString() {
        return String.format("FacilityCheckOut %d (exit %s)", mId, mFacilityNames);
    }
}
