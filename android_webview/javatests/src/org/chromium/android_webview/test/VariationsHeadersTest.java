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

import java.util.List;

/** Tests that the variations headers are correctly set. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@CommandLineFlags.Add({VariationsSwitches.DISABLE_FIELD_TRIAL_TESTING_CONFIG})
public class VariationsHeadersTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public VariationsHeadersTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @MediumTest
    @Test
    public void testGetVariationsHeader() throws Throwable {
        // Check the value is equal to the base64 encoded proto with the forced variations IDs.
        AwContentsStatics.forceVariationIdsForTesting(List.of(), "4,10,34");
        String expectedHeaderStart = "CAQICggi";
        Assert.assertTrue(AwContentsStatics.getVariationsHeader().startsWith(expectedHeaderStart));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.VariationsHeaderLength"));
    }
}
