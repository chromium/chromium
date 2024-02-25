// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

/**
 * Provides a means for validating whether some condition/criteria has been met.
 * <p>
 * See {@link CriteriaHelper} for usage guidelines.
 */
public final class Criteria {
    private Criteria() {}

    /**
     * Validates that a expected condition has been met, and throws an
     * {@link CriteriaNotSatisfiedException} if not.
     *
     * @param <T> The type of value whose being tested.
     * @param actual The actual value being tested.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public static <T> void checkThat(T actual, Matcher<T> matcher) {
        checkThat("", actual, matcher);
    }

    /**
     * Validates that a expected condition has been met, and throws an
     * {@link CriteriaNotSatisfiedException} if not.
     *
     * @param <T> The type of value whose being tested.
     * @param reason Additional reason description for the failure.
     * @param actual The actual value being tested.
     * @param matcher Determines if the current value matches the desired expectation.
     */
    public static <T> void checkThat(String reason, T actual, Matcher<T> matcher) {
        if (matcher.matches(actual)) return;
        Description description = new StringDescription();
        if (!TextUtils.isEmpty(reason)) {
            description.appendText(reason).appendText(System.lineSeparator());
        }
        description
                .appendText("Expected: ")
                .appendDescriptionOf(matcher)
                .appendText(System.lineSeparator())
                .appendText("     but: ");
        matcher.describeMismatch(actual, description);
        throw new CriteriaNotSatisfiedException(description.toString());
    }
}
