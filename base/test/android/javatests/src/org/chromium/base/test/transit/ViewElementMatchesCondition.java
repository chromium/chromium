// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.build.annotations.NullMarked;

/** A {@link Condition} that checks if a {@link ViewElement} matches a {@link Matcher<View>}. */
@NullMarked
public class ViewElementMatchesCondition extends UiThreadCondition {

    private final ViewElement<? extends View> mViewElement;
    private final Matcher<View> mViewMatcher;

    public ViewElementMatchesCondition(
            ViewElement<? extends View> viewElement, Matcher<View> viewMatcher) {
        mViewElement = dependOnSupplier(viewElement, "ViewElement");
        mViewMatcher = viewMatcher;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        View view = mViewElement.get();
        assert view != null;
        if (mViewMatcher.matches(view)) {
            return fulfilled();
        } else {
            StringDescription description = new StringDescription();
            mViewMatcher.describeMismatch(view, description);
            String mismatch = description.toString();
            return notFulfilled(mismatch.isEmpty() ? "does not match " + mViewMatcher : mismatch);
        }
    }

    @Override
    public String buildDescription() {
        return mViewElement.toString() + " matches " + mViewMatcher.toString();
    }
}
