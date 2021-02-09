// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests autofill assistant facade.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantFacadeTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mTestRule.startMainActivityWithURL("about:blank");
    }

    /**
     * Tests that mandatory parameters are indeed mandatory.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    public void
    testMandatoryParameters() {
        Intent intent = new Intent();
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        String extrasPrefix = "org.chromium.chrome.browser.autofill_assistant.";
        intent.putExtra(extrasPrefix + "ENABLED", false);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(extrasPrefix + "ENABLED", true);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(extrasPrefix + "START_IMMEDIATELY", true);
        Assert.assertTrue(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(extrasPrefix + "START_IMMEDIATELY", false);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(extrasPrefix + "REQUEST_TRIGGER_SCRIPT", true);
        Assert.assertTrue(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));
    }

    /**
     * Tests that the preconditions for triggering proactive help work correctly.
     */
    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void proactiveHelpConditions() {
        Assert.assertTrue(AutofillAssistantPreferencesUtil.isProactiveHelpOn());

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false);

        Assert.assertFalse(AutofillAssistantPreferencesUtil.isProactiveHelpOn());

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, false);

        Assert.assertFalse(AutofillAssistantPreferencesUtil.isProactiveHelpOn());

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, true);

        Assert.assertTrue(AutofillAssistantPreferencesUtil.isProactiveHelpOn());
    }
}
