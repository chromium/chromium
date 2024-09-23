// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.components.variations.VariationsSwitches;

/** Tests that the variations headers are correctly set. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@CommandLineFlags.Add({
    VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG,
    VariationsSwitches.FORCE_VARIATION_IDS + "=4,10,34"
})
public class VariationsHeadersTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public VariationsHeadersTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @MediumTest
    @Test
    public void testGetVariationsHeader() throws Throwable {
        // Check the value is equal to the base64 encoded proto with the forced variations IDs.
        String expectedHeader = "CAQICggi";
        Assert.assertEquals(expectedHeader, AwContentsStatics.getVariationsHeader());
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.VariationsHeaderLength", expectedHeader.length()));
    }
}
