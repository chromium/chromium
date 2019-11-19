// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;

/**
 * This class tests AwQuotaManagerBridge runs without AwContents etc. It simulates
 * use case that user calls WebStorage getInstance() without WebView.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class StandaloneAwQuotaManagerBridgeTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    public void testStartup() throws Exception {
        // AwQuotaManager should run without any issue.
        AwQuotaManagerBridge.Origins origins = AwQuotaManagerBridgeTestUtil.getOrigins(
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge());
        Assert.assertEquals(origins.mOrigins.length, 0);
        Assert.assertEquals(AwContents.getNativeInstanceCount(), 0);
    }
}
