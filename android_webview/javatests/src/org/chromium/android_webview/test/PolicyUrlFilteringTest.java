// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.policy.AbstractAppRestrictionsProvider;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.policy.test.PolicyData;
import org.chromium.policy.test.annotations.Policies;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests for the policy based URL filtering. */
@RunWith(AwJUnit4ClassRunner.class)
public class PolicyUrlFilteringTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mFooTestUrl;
    private String mBarTestUrl;
    private static final String sFooTestFilePath = "/foo.html";
    private static final String sFooWhitelistFilter = "localhost" + sFooTestFilePath;

    private static final String sBlacklistPolicyName = "com.android.browser:URLBlacklist";
    private static final String sWhitelistPolicyName = "com.android.browser:URLWhitelist";

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mAwContents = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient)
                              .getAwContents();
        mWebServer = TestWebServer.start();
        mFooTestUrl = mWebServer.setResponse(sFooTestFilePath, "<html><body>foo</body></html>",
                new ArrayList<Pair<String, String>>());
        mBarTestUrl = mWebServer.setResponse("/bar.html", "<html><body>bar</body></html>",
                new ArrayList<Pair<String, String>>());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    // Tests transforming the bundle to native policies, reloading the policies and blocking
    // the navigation.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Policy"})
    // Run in single process only. crbug.com/615484
    @OnlyRunIn(SINGLE_PROCESS)
    @RetryOnFailure
    public void testBlacklistedUrl() throws Throwable {
        final AwPolicyProvider testProvider =
                new AwPolicyProvider(mActivityTestRule.getActivity().getApplicationContext());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> CombinedPolicyProvider.get().registerProvider(testProvider));

        navigateAndCheckOutcome(mFooTestUrl, 0 /* error count before */, 0 /* error count after*/);

        setFilteringPolicy(testProvider, new String[] {"localhost"}, new String[] {});

        navigateAndCheckOutcome(mFooTestUrl, 0 /* error count before */, 1 /* error count after */);
        Assert.assertEquals(ErrorCodeConversionHelper.ERROR_CONNECT,
                mContentsClient.getOnReceivedErrorHelper().getErrorCode());
    }

    // Tests getting a successful navigation with a whitelist.
    // clang-format off
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Policy"})
    @Policies.Add({
            @Policies.Item(key = sBlacklistPolicyName, stringArray = {"*"}),
            @Policies.Item(key = sWhitelistPolicyName, stringArray = {sFooWhitelistFilter})
    })
    @OnlyRunIn(SINGLE_PROCESS) // http://crbug.com/660517
    public void testWhitelistedUrl() throws Throwable {
        navigateAndCheckOutcome(mFooTestUrl, 0 /* error count before */, 0 /* error count after */);

        // Make sure it goes through the blacklist
        navigateAndCheckOutcome(mBarTestUrl, 0 /* error count before */, 1 /* error count after */);
        Assert.assertEquals(ErrorCodeConversionHelper.ERROR_CONNECT,
                mContentsClient.getOnReceivedErrorHelper().getErrorCode());
    }
    // clang-format on

    // Tests that bad policy values are properly handled
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Policy"})
    @Policies.Add({
            @Policies.Item(key = sBlacklistPolicyName, string = "shouldBeAJsonArrayNotAString")})
    public void testBadPolicyValue() throws Exception {
        navigateAndCheckOutcome(mFooTestUrl, 0 /* error count before */, 0 /* error count after */);
        // At the moment this test is written, a failure is a crash, a success is no crash.
    }

    /**
     * Synchronously loads the provided URL and checks that the number or reported errors for the
     * current context is the expected one.
     */
    private void navigateAndCheckOutcome(String url, int startingErrorCount, int expectedErrorCount)
            throws Exception {
        if (expectedErrorCount < startingErrorCount) {
            throw new IllegalArgumentException(
                    "The navigation error count can't decrease over time");
        }
        TestCallbackHelperContainer.OnReceivedErrorHelper onReceivedErrorHelper =
                mContentsClient.getOnReceivedErrorHelper();
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        Assert.assertEquals(startingErrorCount, onReceivedErrorHelper.getCallCount());

        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, url);
        Assert.assertEquals(url, onPageFinishedHelper.getUrl());

        if (expectedErrorCount > startingErrorCount) {
            onReceivedErrorHelper.waitForCallback(
                    startingErrorCount, expectedErrorCount - startingErrorCount);
        }
        Assert.assertEquals(expectedErrorCount, onReceivedErrorHelper.getCallCount());
    }

    private void setFilteringPolicy(final AwPolicyProvider testProvider,
            final String[] blacklistUrls, final String[] whitelistUrls) {
        final PolicyData[] policies = {
            new PolicyData.StrArray(sBlacklistPolicyName, blacklistUrls),
            new PolicyData.StrArray(sWhitelistPolicyName, whitelistUrls)
        };

        AbstractAppRestrictionsProvider.setTestRestrictions(
                PolicyData.asBundle(Arrays.asList(policies)));

        TestThreadUtils.runOnUiThreadBlocking(() -> testProvider.refresh());

        // To avoid race conditions
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
