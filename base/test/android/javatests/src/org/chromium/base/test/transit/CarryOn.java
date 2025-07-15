// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** CarryOn is a lightweight, stand-alone ConditionalState not tied to any Station. */
@NullMarked
public class CarryOn extends ConditionalState {

    private final int mId;
    private final String mName;
    private static int sLastCarryOnId = 2000;

    /**
     * Constructor for named subclasses.
     *
     * <p>Named subclasses should let name default to the simple class name, e.g. "<C1:
     * SubclassNameCarryOn>".
     */
    protected CarryOn() {
        this(/* name= */ null);
    }

    /**
     * Create an empty CarryOn. Elements can be declared after creation.
     *
     * @param name Direct instantiations should provide a name which will be displayed as "<C1:
     *     ProvidedName>",
     */
    public CarryOn(@Nullable String name) {
        mId = ++sLastCarryOnId;
        if (name == null) {
            name = getClass().getSimpleName();
        }

        mName =
                name.isBlank()
                        ? String.format("<C%d>", mId)
                        : String.format("<C%d: %s>", mId, name);
    }

    @Override
    public String getName() {
        return mName;
    }
}
