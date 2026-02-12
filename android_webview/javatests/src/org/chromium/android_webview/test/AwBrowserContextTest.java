// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

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
    public void testSetMaxPrerendersNullDoesNotCrash() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = AwBrowserContext.getDefault();
                    // This should not crash when passing null.
                    context.setMaxPrerenders(null);
                    context.setMaxPrerenders(1);
                    context.setMaxPrerenders(null);
                });
    }
}
