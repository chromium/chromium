// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.assertion.ViewAssertions.matches;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.build.annotations.NullMarked;

/** A {@link Condition} that checks if a {@link ViewElement} matches a {@link Matcher<View>}. */
@NullMarked
public class ViewElementMatchesCondition extends InstrumentationThreadCondition {

    private final ViewElement<? extends View> mViewElement;
    private final Matcher<View> mViewMatcher;

    public ViewElementMatchesCondition(
            ViewElement<? extends View> viewElement, Matcher<View> viewMatcher) {
        mViewElement = dependOnSupplier(viewElement, "ViewElement");
        mViewMatcher = viewMatcher;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        try {
            mViewElement.check(matches(mViewMatcher));
            return fulfilled();
        } catch (AssertionError e) {
            return notFulfilled(e.getMessage());
        }
    }

    @Override
    public String buildDescription() {
        return mViewElement.toString() + " matches " + mViewMatcher.toString();
    }
}
