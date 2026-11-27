// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * State is a lightweight ConditionalState not tied to any Station.
 *
 * <p>A State can have another ConditionalState as parent: exiting the parent also exits the State
 * if it is still ACTIVE, and the State uses the parent's Activity if it declares none.
 */
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

    /**
     * Set the parent of this State. Must be called before entering this State.
     *
     * <p>Exiting the parent also exits this State if it is still ACTIVE, and this State uses the
     * parent's Activity if it declares none.
     */
    @Override
    public void setParent(ConditionalState parent) {
        super.setParent(parent);
    }

    @Override
    @Nullable ActivityElement<?> determineActivityElement() {
        return mUntypedActivityElement != null
                ? mUntypedActivityElement
                : super.determineActivityElement();
    }

    @Override
    <T extends Activity> void onDeclaredActivityElement(ActivityElement<T> element) {
        assert mUntypedActivityElement == null
                : String.format(
                        "%s already declared an ActivityElement with id %s",
                        getName(), mUntypedActivityElement.getId());
        mUntypedActivityElement = element;
    }
}
