// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrCardboardTestRule;
import org.chromium.chrome.browser.vr.rules.CustomTabActivityVrCardboardTestRule;
import org.chromium.chrome.browser.vr.rules.VrTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityVrCardboardTestRule;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Utility class for interacting with Cardboard VR-specific Rules, i.e. ChromeActivityTestRules that
 * implement the VrTestRule interface.
 */
public class VrCardboardTestRuleUtils extends XrTestRuleUtils {
    /**
     * Helper method to apply a VrTestRule/ChromeActivityTestRule combination. The only difference
     * between various classes that implement VrTestRule is how they start their activity, so the
     * common boilerplate code can be kept here so each VrTestRule only has to provide a way to
     * launch Chrome.
     *
     * @param base The Statement passed to the calling ChromeActivityTestRule's apply() method.
     * @param desc The Description passed to the calling ChromeActivityTestRule's apply() method.
     * @param rule The calling VrTestRule.
     * @param launcher A ChromeLaunchMethod whose launch() contains the code snippet to start Chrome
     *     in the calling ChromeActivityTestRule's activity type.
     */
    public static void evaluateVrTestRuleImpl(
            final Statement base,
            final Description desc,
            final VrTestRule rule,
            final ChromeLaunchMethod launcher)
            throws Throwable {
        launcher.launch();

        base.evaluate();
    }

    /**
     * Creates the list of VrTestRules that are currently supported for use in test
     * parameterization.
     *
     * @return An ArrayList of ParameterSets, with each ParameterSet containing a callable to create
     *     a VrTestRule for a supported ChromeActivity.
     */
    public static ArrayList<ParameterSet> generateDefaultTestRuleParameters() {
        ArrayList<ParameterSet> parameters = new ArrayList<ParameterSet>();
        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<ChromeTabbedActivityVrCardboardTestRule>() {
                                    @Override
                                    public ChromeTabbedActivityVrCardboardTestRule call() {
                                        return new ChromeTabbedActivityVrCardboardTestRule();
                                    }
                                })
                        .name("ChromeTabbedActivity"));

        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<CustomTabActivityVrCardboardTestRule>() {
                                    @Override
                                    public CustomTabActivityVrCardboardTestRule call() {
                                        return new CustomTabActivityVrCardboardTestRule();
                                    }
                                })
                        .name("CustomTabActivity"));

        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<WebappActivityVrCardboardTestRule>() {
                                    @Override
                                    public WebappActivityVrCardboardTestRule call() {
                                        return new WebappActivityVrCardboardTestRule();
                                    }
                                })
                        .name("WebappActivity"));

        return parameters;
    }
}
