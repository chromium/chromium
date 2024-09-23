// Copyright 2022 The Chromium Authors
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
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.ShouldInterceptRequestHelper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.io.ByteArrayInputStream;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/**
 * Integration test for persistent origin trials in WebView.
 *
 * <p>Due to the difficulty of testing origin trials in the context of webview, this test suite will
 * only contain enough tests to verify that the persistent origin trial components are loaded.
 */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({
    "origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=",
    "enable-features=PersistentOriginTrials"
})
public class AwPersistentOriginTrialTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    // Must match the native |origin_trials::kOriginTrialPrefKey|
    private static final String ORIGIN_TRIAL_PREFERENCE_KEY = "origin_trials.persistent_trials";

    private static final String ORIGIN_TRIAL_HEADER = "Origin-Trial";
    private static final String CRITICAL_ORIGIN_TRIAL_HEADER = "Critical-Origin-Trial";

    private static final String PERSISTENT_TRIAL_NAME = "FrobulatePersistent";

    /*
    Generated with
    tools/origin_trials/generate_token.py https://example.com FrobulatePersistent \
    --expire-timestamp=2000000000
     */
    private static final String PERSISTENT_TRIAL_TOKEN =
            "AzZfd1vKZ0SSGRGk/8nIszQSlHYjbuYVE3jwaNZG3X4t11zRhzPWWJwTZ+JJDS3JJsyEZcpz+y20pAP6/6upOQ"
                + "4AAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdG"
                + "VQZXJzaXN0ZW50IiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";
    private ShouldInterceptRequestHelper mInterceptRequestHelper;

    public AwPersistentOriginTrialTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mInterceptRequestHelper = mContentsClient.getShouldInterceptRequestHelper();
    }

    @After
    public void tearDown() throws Exception {
        // Clean up the stored tokens after tests
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AwBrowserContext context = mActivityTestRule.getAwBrowserContext();
                    context.clearPersistentOriginTrialStorageForTesting();
                });
    }

    @Test
    @SmallTest
    public void testCriticalHeaderCausesRetry() throws Throwable {
        final String requestUrl = "https://example.com/";
        var headers =
                Map.of(
                        ORIGIN_TRIAL_HEADER,
                        PERSISTENT_TRIAL_TOKEN,
                        CRITICAL_ORIGIN_TRIAL_HEADER,
                        PERSISTENT_TRIAL_NAME);
        var body =
                new ByteArrayInputStream(
                        "<!DOCTYPE html><html><body>Hello, World".getBytes(StandardCharsets.UTF_8));
        var responseInfo =
                new WebResourceResponseInfo("text/html", "utf-8", body, 200, "OK", headers);
        mInterceptRequestHelper.setReturnValueForUrl(requestUrl, responseInfo);

        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, requestUrl);
        Assert.assertEquals(requestUrl, onPageFinishedHelper.getUrl());

        // We expect two requests to be made due to the retry.
        Assert.assertEquals(2, mInterceptRequestHelper.getRequestCountForUrl(requestUrl));
    }
}
