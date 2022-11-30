// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.autofill_assistant.TriggerContext;

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
    @UiThreadTest
    public void triggerContextEnabled() {
        Assert.assertFalse(TriggerContext.newBuilder().build().isEnabled());
        Assert.assertFalse(
                TriggerContext.newBuilder().addParameter("ENABLED", false).build().isEnabled());
        Assert.assertTrue(
                TriggerContext.newBuilder().addParameter("ENABLED", true).build().isEnabled());

        // Stringified boolean is not allowed.
        Assert.assertFalse(
                TriggerContext.newBuilder().addParameter("ENABLED", "true").build().isEnabled());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void decodeExperimentIds() {
        Bundle testBundleSingleExperimentId = new Bundle();
        testBundleSingleExperimentId.putString(
                "org.chromium.chrome.browser.autofill_assistant.EXPERIMENT_IDS", "123");
        Assert.assertEquals("123",
                TriggerContext.newBuilder()
                        .fromBundle(testBundleSingleExperimentId)
                        .build()
                        .getExperimentIds());

        Bundle testBundleMultipleExperimentIds = new Bundle();
        testBundleMultipleExperimentIds.putString(
                "org.chromium.chrome.browser.autofill_assistant.EXPERIMENT_IDS", "123%2C456");
        Assert.assertEquals("123,456",
                TriggerContext.newBuilder()
                        .fromBundle(testBundleMultipleExperimentIds)
                        .build()
                        .getExperimentIds());

        // Invalid entries should be passed on without error.
        Bundle testBundleWithInvalidExperimentId = new Bundle();
        testBundleWithInvalidExperimentId.putString(
                "org.chromium.chrome.browser.autofill_assistant.EXPERIMENT_IDS",
                "123%2Cinvalid%2C456");
        Assert.assertEquals("123,invalid,456",
                TriggerContext.newBuilder()
                        .fromBundle(testBundleWithInvalidExperimentId)
                        .build()
                        .getExperimentIds());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void decodeStringParameters() {
        Bundle testBundle = new Bundle();
        testBundle.putString("org.chromium.chrome.browser.autofill_assistant.FAKE_PARAM",
                "https%3A%2F%2Fwww.example.com");
        Assert.assertEquals("https://www.example.com",
                TriggerContext.newBuilder()
                        .fromBundle(testBundle)
                        .build()
                        .getParameters()
                        .get("FAKE_PARAM"));
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void deviceOnlyParameters() {
        Bundle testBundle = new Bundle();
        testBundle.putString("org.chromium.chrome.browser.autofill_assistant.PARAM_A", "public");
        testBundle.putString(
                "org.chromium.chrome.browser.autofill_assistant.device_only.PARAM_B", "secret");
        TriggerContext triggerContext = TriggerContext.newBuilder().fromBundle(testBundle).build();
        Assert.assertEquals("public", triggerContext.getParameters().get("PARAM_A"));
        Assert.assertEquals("secret", triggerContext.getDeviceOnlyParameters().get("PARAM_B"));
    }
}
