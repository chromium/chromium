// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.Nullable;

import org.hamcrest.CoreMatchers;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.Promise;

/** Helper class containing Hamcrest matchers. */
public class Matchers extends CoreMatchers {
    private static class GreaterThanOrEqualTo<T extends Comparable<T>> extends TypeSafeMatcher<T> {

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

    /**
     * @param <T> The type of the {@link Promise} to match.
     * @return A matcher that expects {@link Promise#isPending()} to be true.
     */
    public static <T> Matcher<Promise<T>> pendingPromise() {
        return new PromiseMatcher<>(PromiseMatcher.ExpectedStatus.PENDING, null);
    }

    /**
     * @param <T> The type of the {@link Promise} to match.
     * @return A matcher that expects {@link Promise#isRejected()} to be true.
     */
    public static <T> Matcher<Promise<T>> rejectedPromise() {
        return new PromiseMatcher<>(PromiseMatcher.ExpectedStatus.REJECTED, null);
    }

    /**
     * @param <T> The type of the {@link Promise} to match.
     * @return A matcher that expects {@link Promise#isFulfilled()} to be true.
     */
    public static <T> Matcher<Promise<T>> fulfilledPromise() {
        return new PromiseMatcher<>(PromiseMatcher.ExpectedStatus.FULFILLED, null);
    }

    /**
     * @param <T> The type of the {@link Promise} to match.
     * @param resultMatcher The matcher for the promise's result.
     * @return A matcher that expects {@link Promise#isFulfilled()} to be true and that {@link
     *     Promise#getResult()} can be matched by {@code resultMatcher}.
     */
    public static <T> Matcher<Promise<T>> fulfilledPromise(Matcher<T> resultMatcher) {
        return new PromiseMatcher<>(PromiseMatcher.ExpectedStatus.FULFILLED, resultMatcher);
    }

    private static class PromiseMatcher<T> extends TypeSafeMatcher<Promise<T>> {
        private enum ExpectedStatus {
            PENDING,
            REJECTED,
            FULFILLED
        }

        private final ExpectedStatus mExpectedStatus;
        private final @Nullable Matcher<T> mResultMatcher;

        private PromiseMatcher(ExpectedStatus expectedStatus, @Nullable Matcher<T> resultMatcher) {
            assert resultMatcher == null || expectedStatus == ExpectedStatus.FULFILLED;

            mExpectedStatus = expectedStatus;
            mResultMatcher = resultMatcher;
        }

        @Override
        protected void describeMismatchSafely(Promise<T> item, Description mismatchDescription) {
            if (item.isPending()) {
                mismatchDescription.appendText("is pending");
            } else if (item.isRejected()) {
                mismatchDescription.appendText("is rejected");
            } else {
                mismatchDescription.appendText(
                        String.format("is fulfilled and result is \"%s\"", item.getResult()));
            }
        }

        @Override
        public void describeTo(Description description) {
            switch (mExpectedStatus) {
                case PENDING -> description.appendText("pending");
                case REJECTED -> description.appendText("rejected");
                case FULFILLED -> {
                    description.appendText("fulfilled");
                    if (mResultMatcher != null) {
                        description.appendText(" and result is ");
                        mResultMatcher.describeTo(description);
                    }
                }
            }
        }

        @Override
        protected boolean matchesSafely(Promise<T> promise) {
            return switch (mExpectedStatus) {
                case PENDING -> promise.isPending();
                case REJECTED -> promise.isRejected();
                case FULFILLED -> {
                    if (!promise.isFulfilled()) {
                        yield false;
                    }
                    yield mResultMatcher == null || mResultMatcher.matches(promise.getResult());
                }
            };
        }
    }
}
