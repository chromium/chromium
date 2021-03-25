// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for TriggerContext.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TriggerContextTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        mTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT)
    public void testRegularScriptContext() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(TriggerContext.newBuilder().build().isValid());

            Assert.assertFalse(
                    TriggerContext.newBuilder().addParameter("ENABLED", true).build().isValid());

            Assert.assertTrue(TriggerContext.newBuilder()
                                      .addParameter("ENABLED", true)
                                      .addParameter("START_IMMEDIATELY", true)
                                      .build()
                                      .isValid());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    public void
    testTriggerScriptContext() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Enable MSBB.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), true);
            Assert.assertFalse(TriggerContext.newBuilder()
                                       .addParameter("ENABLED", true)
                                       .addParameter("START_IMMEDIATELY", false)
                                       .build()
                                       .isValid());

            Assert.assertTrue(TriggerContext.newBuilder()
                                      .addParameter("ENABLED", true)
                                      .addParameter("START_IMMEDIATELY", false)
                                      .addParameter("REQUEST_TRIGGER_SCRIPT", true)
                                      .build()
                                      .isValid());

            // Disable MSBB. Base64 trigger scripts should not require it.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), false);
            Assert.assertTrue(TriggerContext.newBuilder()
                                      .addParameter("ENABLED", true)
                                      .addParameter("START_IMMEDIATELY", false)
                                      .addParameter("TRIGGER_SCRIPTS_BASE64", "abcd")
                                      .build()
                                      .isValid());
        });
    }
}
