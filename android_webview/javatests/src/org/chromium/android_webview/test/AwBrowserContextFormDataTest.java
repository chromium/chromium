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

/**
 * Tests the methods on AwBrowserContext that expose the Chromium form database for the WebView API.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwBrowserContextFormDataTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

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
