// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import org.junit.Assume;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.XrTestRuleUtils;

/**
 * Rule that conditionally skips a test if the current VrTestRule's Activity is not
 * one of the supported Activity types for the test.
 */
public class VrActivityRestrictionRule implements TestRule {
    private @SupportedActivity int mCurrentRestriction;

    public VrActivityRestrictionRule(@SupportedActivity int currentRestriction) {
        mCurrentRestriction = currentRestriction;
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        // Currently, we don't have any VR-specific logic except for standalone devices.
        if (!TestVrShellDelegate.isOnStandalone()) {
            return base;
        }
        // We can only run tests in ChromeTabbedActivity on standalones, so ignore if the current
        // activity isn't a CTA. XrActivityRestrictionRule will take care of ensuring that the test
        // actually supports running in a CTA.
        return mCurrentRestriction == SupportedActivity.CTA ? base
                                                            : generateStandaloneIgnoreStatement();
    }

    private Statement generateStandaloneIgnoreStatement() {
        return new Statement() {
            @Override
            public void evaluate() {
                Assume.assumeTrue("Test ignored because "
                                + XrTestRuleUtils.supportedActivityToString(mCurrentRestriction)
                                + " is not a supported activity on standalone devices.",
                        false);
            }
        };
    }
}
