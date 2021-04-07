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
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
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
    public void triggerContextEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(TriggerContext.newBuilder().build().isEnabled());
            Assert.assertFalse(
                    TriggerContext.newBuilder().addParameter("ENABLED", false).build().isEnabled());
            Assert.assertTrue(
                    TriggerContext.newBuilder().addParameter("ENABLED", true).build().isEnabled());
        });
    }
}
