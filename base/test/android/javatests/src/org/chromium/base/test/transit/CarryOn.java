// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.build.annotations.NullMarked;

/** CarryOn is a lightweight, stand-alone ConditionalState not tied to any Station. */
@NullMarked
public class CarryOn extends ConditionalState {

    private final int mId;
    private final String mName;
    private static int sLastCarryOnId = 2000;

    public CarryOn() {
        mId = ++sLastCarryOnId;
        String className = getClass().getSimpleName();
        mName =
                className.isBlank()
                        ? String.format("<C%d>", mId)
                        : String.format("<C%d: %s>", mId, className);
    }

    @Override
    public String getName() {
        return mName;
    }
}
