// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import org.hamcrest.Matcher;

/** A {@link Condition} that checks if a {@link ViewElement} matches a {@link Matcher<View>}. */
public class ViewElementMatchesCondition extends InstrumentationThreadCondition {

    private ViewElement mViewElement;
    private Matcher<View> mViewMatcher;

    public ViewElementMatchesCondition(ViewElement viewElement, Matcher<View> viewMatcher) {
        mViewElement = dependOnSupplier(viewElement, "ViewElement");
        mViewMatcher = viewMatcher;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        return whether(mViewMatcher.matches(mViewElement.get()));
    }

    @Override
    public String buildDescription() {
        return mViewElement.toString() + " matches " + mViewMatcher.toString();
    }
}
