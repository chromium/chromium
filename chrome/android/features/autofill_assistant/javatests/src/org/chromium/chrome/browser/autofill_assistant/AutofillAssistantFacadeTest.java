// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Function;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.external_intents.ExternalNavigationDelegate.IntentToAutofillAllowingAppResult;

/**
 * Tests autofill assistant facade.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantFacadeTest {
    private static final String EXTRAS_PREFIX = "org.chromium.chrome.browser.autofill_assistant.";

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

        intent.putExtra(EXTRAS_PREFIX + "ENABLED", false);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(EXTRAS_PREFIX + "ENABLED", true);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(EXTRAS_PREFIX + "START_IMMEDIATELY", true);
        Assert.assertTrue(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(EXTRAS_PREFIX + "START_IMMEDIATELY", false);
        Assert.assertFalse(AutofillAssistantFacade.isAutofillAssistantEnabled(intent));

        intent.putExtra(EXTRAS_PREFIX + "REQUEST_TRIGGER_SCRIPT", true);
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

    /**
     * Tests that the app override signal works as expected.
     */
    @Test
    @MediumTest
    public void testAppOverrideSignal() {
        Function<Intent, Boolean> hasNoHandler = (i) -> false;
        Function<Intent, Boolean> hasHandler = (i) -> true;

        Intent intent = new Intent();
        intent.setData(Uri.parse("https://example.com/"));
        Assert.assertEquals(AutofillAssistantFacade.shouldAllowOverrideWithApp(intent, hasHandler),
                IntentToAutofillAllowingAppResult.NONE);

        intent.putExtra(EXTRAS_PREFIX + "ALLOW_APP", "false");
        Assert.assertEquals(AutofillAssistantFacade.shouldAllowOverrideWithApp(intent, hasHandler),
                IntentToAutofillAllowingAppResult.NONE);

        intent.putExtra(EXTRAS_PREFIX + "ALLOW_APP", "true");
        Assert.assertEquals(
                AutofillAssistantFacade.shouldAllowOverrideWithApp(intent, hasNoHandler),
                IntentToAutofillAllowingAppResult.NONE);
        Assert.assertEquals(AutofillAssistantFacade.shouldAllowOverrideWithApp(intent, hasHandler),
                IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW);

        intent.setData(Uri.parse("https://redirect.com/"));
        intent.putExtra(EXTRAS_PREFIX + "ORIGINAL_DEEPLINK", "https://example.com");
        Assert.assertEquals(
                AutofillAssistantFacade.shouldAllowOverrideWithApp(intent, hasNoHandler),
                IntentToAutofillAllowingAppResult.NONE);
        Assert.assertEquals(AutofillAssistantFacade.shouldAllowOverrideWithApp(
                                    intent, (i) -> i.getDataString().contains("example")),
                IntentToAutofillAllowingAppResult.DEFER_TO_APP_LATER);
    }
}
