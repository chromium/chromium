// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;

/**
 * This class tests AwQuotaManagerBridge runs without AwContents etc. It simulates use case that
 * user calls WebStorage getInstance() without WebView.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class StandaloneAwQuotaManagerBridgeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public StandaloneAwQuotaManagerBridgeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    public void testStartup() throws Exception {
        // AwQuotaManager should run without any issue.
        AwQuotaManagerBridge.Origins origins =
                AwQuotaManagerBridgeTestUtil.getOrigins(
                        mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge());
        Assert.assertEquals(origins.mOrigins.length, 0);
        Assert.assertEquals(AwContents.getNativeInstanceCount(), 0);
    }
}
