// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

import java.util.Collections;

/**
 * Tests for WebEnvironmentIntegrity in WebView.
 *
 * These tests are in addition to
 * {@link org.chromium.chrome.browser.environment_integrity.EnvironmentIntegrityTest}
 * and only supposed to test WebView-specific differences.
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add({"enable-features=" + ContentFeatures.WEB_ENVIRONMENT_INTEGRITY})
@Batch(Batch.PER_CLASS)
public class AwWebEnvironmentIntegrityTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();

        mContentsClient = new TestAwContentsClient();
        AwTestContainerView mContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    @After
    public void tearDown() throws Exception {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    public void testWebEnvironmentIntegrityApiAvailable() throws Throwable {
        // Load a web page from localhost to get a secure context
        mWebServer.setResponse("/", "<html>", Collections.emptyList());
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mWebServer.getBaseUrl());
        // Check that the 'getEnvironmentIntegrity' method is available.
        final String script = "'getEnvironmentIntegrity' in navigator ? 'available': 'missing'";
        String result = mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, script);
        // The result is expected to have extra quotes as a JSON-encoded string.
        Assert.assertEquals("\"available\"", result);
    }
}
