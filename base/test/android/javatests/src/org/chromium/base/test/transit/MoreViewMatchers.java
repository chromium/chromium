// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;

import static org.hamcrest.Matchers.is;

import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;

import org.chromium.build.annotations.NullMarked;

/** Additional {@link View} matchers complementing {@link ViewMatchers}. */
@NullMarked
public class MoreViewMatchers {
    /**
     * Returns a matcher that matches {@link View}s that are descendants of the given {@code
     * ancestor} {@link View} instance.
     *
     * <p>Shorthand for {@code isDescendantOfA(is(ancestor))}.
     */
    public static Matcher<View> isDescendantOf(View ancestor) {
        return isDescendantOfA(is(ancestor));
    }
}
