// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.os.SystemClock;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.BundleUtils;
import org.chromium.base.test.BundleTestRule;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityGvrTestRule;
import org.chromium.chrome.browser.vr.rules.VrTestRule;
import org.chromium.chrome.browser.vr.rules.WebappActivityGvrTestRule;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/**
 * Utility class for interacting with VR-specific Rules, i.e. ChromeActivityTestRules that implement
 * the VrTestRule interface.
 */
public class GvrTestRuleUtils extends XrTestRuleUtils {
    // VrCore waits this amount of time after exiting VR before actually unregistering a registered
    // Daydream intent, meaning that it still thinks VR is active until this amount of time has
    // passed.
    private static final int VRCORE_UNREGISTER_DELAY_MS = 500;

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
        TestVrShellDelegate.setDescription(desc);

        GvrTestRuleUtils.ensureNoVrActivitiesDisplayed();
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
                                new Callable<ChromeTabbedActivityGvrTestRule>() {
                                    @Override
                                    public ChromeTabbedActivityGvrTestRule call() {
                                        return new ChromeTabbedActivityGvrTestRule();
                                    }
                                })
                        .name("ChromeTabbedActivity"));
        // TODO(https://crbug.com/989117): Re-enable testing in CCT once we've migrated to using
        //    cardboard libraries instead of Daydream.
        // parameters.add(new ParameterSet()
        //                        .value(new Callable<CustomTabActivityGvrTestRule>() {
        //                            @Override
        //                            public CustomTabActivityGvrTestRule call() {
        //                                return new CustomTabActivityGvrTestRule();
        //                            }
        //                        })
        //                        .name("CustomTabActivity"));

        parameters.add(
                new ParameterSet()
                        .value(
                                new Callable<WebappActivityGvrTestRule>() {
                                    @Override
                                    public WebappActivityGvrTestRule call() {
                                        return new WebappActivityGvrTestRule();
                                    }
                                })
                        .name("WebappActivity"));

        return parameters;
    }

    /**
     * Creates a RuleChain that applies the XrActivityRestrictionRule before the given VrTestRule.
     *
     * <p>Also enforces that {@link BundleUtils#isBundle()} returns true for vr to be initialized.
     *
     * @param rule The TestRule to wrap in an XrActivityRestrictionRule.
     * @return A RuleChain that ensures an XrActivityRestrictionRule is applied before the provided
     *     TestRule.
     */
    public static RuleChain wrapRuleInActivityRestrictionRule(TestRule rule) {
        Assert.assertTrue("Given rule is not an VrTestRule", rule instanceof VrTestRule);
        return RuleChain.outerRule(new BundleTestRule())
                .around(XrTestRuleUtils.wrapRuleInActivityRestrictionRule(rule));
    }

    /**
     * Ensures that no VR-related activity is currently being displayed. This is meant to be used by
     * TestRules before starting any activities. Having a VR activity in the foreground (e.g.
     * Daydream Home) has the potential to affect test results, as it often means that we are in VR
     * at the beginning of the test, which we don't want. This is most commonly caused by VrCore
     * automatically launching Daydream Home when Chrome gets closed after a test, but can happen
     * for other reasons as well.
     */
    public static void ensureNoVrActivitiesDisplayed() {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String currentPackageName = uiDevice.getCurrentPackageName();
        if (currentPackageName != null && currentPackageName.contains("vr")) {
            uiDevice.pressHome();
            // Chrome startup would likely be slow enough that this sleep is unnecessary, but sleep
            // to be sure since this will be hit relatively infrequently.
            SystemClock.sleep(VRCORE_UNREGISTER_DELAY_MS);
        }
    }
}
