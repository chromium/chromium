// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

/** AwBrowserContext tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwBrowserContextTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwBrowserContextTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSetAndGetMaxPrerenders() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = AwBrowserContext.getDefault();
                    context.setMaxPrerenders(1);
                    Assert.assertEquals(1, context.getAllowedPrerenderingCount());
                    context.setMaxPrerenders(3);
                    Assert.assertEquals(3, context.getAllowedPrerenderingCount());

                    // This test checks that we can not set prerenders more than the absolute max
                    // that we can set, currently set in
                    // android_webview/browser/aw_browser_context.h#kMaxAllowedPrerenderingCount
                    context.setMaxPrerenders(4);
                    Assert.assertEquals(3, context.getAllowedPrerenderingCount());

                    // Currently the maximum prerendering count is 2
                    context.clearMaxPrerenders();
                    Assert.assertEquals(2, context.getAllowedPrerenderingCount());
                });
    }
}
