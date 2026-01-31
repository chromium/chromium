// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** State is a lightweight, stand-alone ConditionalState not tied to any Station. */
@NullMarked
public class State extends ConditionalState {

    private final int mId;
    private final String mName;
    private static int sLastStateId = 2000;
    private @Nullable ActivityElement<?> mUntypedActivityElement;

    /**
     * Constructor for named subclasses.
     *
     * <p>Named subclasses should let name default to the simple class name, e.g. "<C1:
     * SubclassName>".
     */
    protected State() {
        this(/* name= */ null);
    }

    /**
     * Create an empty State. Elements can be declared after creation.
     *
     * @param name Direct instantiations should provide a name which will be displayed as "<C1:
     *     ProvidedName>",
     */
    public State(@Nullable String name) {
        mId = ++sLastStateId;
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

    @Override
    @Nullable ActivityElement<?> determineActivityElement() {
        return mUntypedActivityElement;
    }

    @Override
    <T extends Activity> void onDeclaredActivityElement(ActivityElement<T> element) {
        ActivityElement<?> existingActivityElement = determineActivityElement();
        assert existingActivityElement == null
                : String.format(
                        "%s already declared an ActivityElement with id %s",
                        getName(), existingActivityElement.getId());
        mUntypedActivityElement = element;
    }
}
