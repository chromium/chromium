// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

/** Tests the methods on AwBrowserContext that expose the Chromium form database for the WebView API. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwBrowserContextFormDataTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwBrowserContextFormDataTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    public void testSmoke() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mActivityTestRule.getAwBrowserContext().clearFormData();
                            Assert.assertFalse(
                                    mActivityTestRule.getAwBrowserContext().hasFormData());
                        });
    }
}
