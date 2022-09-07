// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.hamcrest.CoreMatchers;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

/**
 * Helper class containing Hamcrest matchers.
 */
public class Matchers extends CoreMatchers {
    private static class GreaterThanOrEqualTo<T extends Comparable<T>>
            extends TypeSafeMatcher<T> {

        private final T mComparisonValue;

        public GreaterThanOrEqualTo(T comparisonValue) {
            mComparisonValue = comparisonValue;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("greater than or equal to ").appendValue(mComparisonValue);
        }

        @Override
        protected boolean matchesSafely(T item) {
            return item.compareTo(mComparisonValue) >= 0;
        }
    }

    /**
     * @param <T> A Comparable type.
     * @param comparisonValue The value to be compared against.
     * @return A matcher that expects the value to be greater than the |comparisonValue|.
     */
    public static <T extends Comparable<T>> Matcher<T> greaterThanOrEqualTo(T comparisonValue) {
        return new GreaterThanOrEqualTo<>(comparisonValue);
    }
}
