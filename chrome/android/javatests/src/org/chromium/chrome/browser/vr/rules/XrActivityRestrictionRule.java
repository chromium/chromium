// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.Assume;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.XrTestRuleUtils;

/**
 * Rule that conditionally skips a test if the current XrTestRule's Activity is not
 * one of the supported Activity types for the test.
 */
public class XrActivityRestrictionRule implements TestRule {
    private @SupportedActivity int mCurrentRestriction;

    public XrActivityRestrictionRule(@SupportedActivity int currentRestriction) {
        mCurrentRestriction = currentRestriction;
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        // Check if the test has a XrActivityRestriction annotation
        XrActivityRestriction annotation = desc.getAnnotation(XrActivityRestriction.class);
        if (annotation == null) {
            if (mCurrentRestriction == SupportedActivity.CTA) {
                // Default to running in ChromeTabbedActivity if no restriction annotation
                return base;
            }
            return generateIgnoreStatement();
        }

        @SupportedActivity
        int[] activities = annotation.value();
        for (int i = 0; i < activities.length; i++) {
            if (activities[i] == mCurrentRestriction || activities[i] == SupportedActivity.ALL) {
                return base;
            }
        }
        return generateIgnoreStatement();
    }

    private Statement generateIgnoreStatement() {
        return new Statement() {
            @Override
            public void evaluate() {
                Assume.assumeTrue("Test ignored because "
                                + XrTestRuleUtils.supportedActivityToString(mCurrentRestriction)
                                + " was not one of the specified activities to run the test in.",
                        false);
            }
        };
    }
}
