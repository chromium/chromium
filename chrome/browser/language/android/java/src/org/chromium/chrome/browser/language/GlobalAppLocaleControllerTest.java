// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.language.AndroidLanguageMetricsBridge;

/**
 * Tests for the GlobalAppLocaleController class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GlobalAppLocaleControllerTest {
    private static final int EMPTY_STRING_HASH = -1895779836;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    public void testStartupHistograms() {
        CriteriaHelper.pollUiThread(() -> {
            // The initial app language is the default system language recorded as the empty string.
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AndroidLanguageMetricsBridge.OVERRIDE_LANGUAGE_HISTOGRAM,
                            EMPTY_STRING_HASH));
        });
    }
}
