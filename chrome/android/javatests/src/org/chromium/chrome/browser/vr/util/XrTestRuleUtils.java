// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.junit.Assert;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityXrTestRule;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.rules.XrActivityRestrictionRule;
import org.chromium.chrome.browser.vr.rules.XrTestRule;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Utility class for interacting with XR-specific Rules, i.e. ChromeActivityTestRules that implement
 * the XrTestRule interface.
 */
public class XrTestRuleUtils {
    /** Essentially a Runnable that can throw exceptions. */
    public interface ChromeLaunchMethod {
        public void launch() throws Throwable;
    }

    /**
     * Creates the list of XrTestRules that are currently supported for use in test
     * parameterization.
     *
     * @return An ArrayList of ParameterSets, with each ParameterSet containing a callable to create
     *     an XrTestRule for a supported ChromeActivity.
     */
    public static ArrayList<ParameterSet> generateDefaultTestRuleParameters() {
        ArrayList<ParameterSet> parameters = new ArrayList<ParameterSet>();
        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<ChromeTabbedActivityXrTestRule>() {
                                    @Override
                                    public ChromeTabbedActivityXrTestRule call() {
                                        return new ChromeTabbedActivityXrTestRule();
                                    }
                                })
                        .name("ChromeTabbedActivity"));

        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<CustomTabActivityXrTestRule>() {
                                    @Override
                                    public CustomTabActivityXrTestRule call() {
                                        return new CustomTabActivityXrTestRule();
                                    }
                                })
                        .name("CustomTabActivity"));

        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<WebappActivityXrTestRule>() {
                                    @Override
                                    public WebappActivityXrTestRule call() {
                                        return new WebappActivityXrTestRule();
                                    }
                                })
                        .name("WebappActivity"));

        return parameters;
    }

    /**
     * Creates a RuleChain that applies the XrActivityRestrictionRule before the given XrTestRule.
     *
     * @param rule The TestRule to wrap in an XrActivityRestrictionRule.
     * @return A RuleChain that ensures an XrActivityRestrictionRule is applied before the provided
     *     TestRule.
     */
    public static RuleChain wrapRuleInActivityRestrictionRule(TestRule rule) {
        Assert.assertTrue("Given rule is not an XrTestRule", rule instanceof XrTestRule);
        return RuleChain.outerRule(
                        new XrActivityRestrictionRule(((XrTestRule) rule).getRestriction()))
                .around(rule);
    }

    /**
     * Converts XrActivityRestriction.SupportedActivity enum to strings.
     *
     * @param activity The SupportedActivity value to convert to a String.
     * @return A String representation of the activity.
     */
    public static String supportedActivityToString(@SupportedActivity int activity) {
        switch (activity) {
            case SupportedActivity.CTA:
                return "ChromeTabbedActivity";
            case SupportedActivity.CCT:
                return "CustomTabActivity";
            case SupportedActivity.WAA:
                return "WebappActivity";
            case SupportedActivity.ALL:
                return "AllActivities";
            default:
                return "UnknownActivity";
        }
    }
}
