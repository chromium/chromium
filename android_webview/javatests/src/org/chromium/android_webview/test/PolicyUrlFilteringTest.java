// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.test.PolicyData;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests for the policy based URL filtering. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class PolicyUrlFilteringTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mFooTestUrl;
    private String mBarTestUrl;
    private static final String sFooTestFilePath = "/foo.html";
    private static final String sFooAllowlistFilter = "localhost" + sFooTestFilePath;

    private static final String sBlocklistPolicyName = "com.android.browser:URLBlocklist";
    private static final String sAllowlistPolicyName = "com.android.browser:URLAllowlist";

    public PolicyUrlFilteringTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        mWebServer = TestWebServer.start();
        mFooTestUrl =
                mWebServer.setResponse(
                        sFooTestFilePath,
                        "<html><body>foo</body></html>",
                        new ArrayList<Pair<String, String>>());
        mBarTestUrl =
                mWebServer.setResponse(
                        "/bar.html",
                        "<html><body>bar</body></html>",
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
    @DisabledTest(message = "crbug.com/623586")
    public void testBlocklistedUrl() throws Throwable {
        final AwPolicyProvider testProvider =
                new AwPolicyProvider(mActivityTestRule.getActivity().getApplicationContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> CombinedPolicyProvider.get().registerProvider(testProvider));

        navigateAndCheckOutcome(
                mFooTestUrl, /* startingErrorCount= */ 0, /* expectedErrorCount= */ 0);

        setFilteringPolicy(testProvider, new String[] {"localhost"}, new String[] {});

        navigateAndCheckOutcome(
                mFooTestUrl, /* startingErrorCount= */ 0, /* expectedErrorCount= */ 1);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_CONNECT,
                mContentsClient.getOnReceivedErrorHelper().getError().errorCode);
    }

    // Tests getting a successful navigation with an allowlist.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "Policy"})
    @Policies.Add({
        @Policies.Item(
                key = sBlocklistPolicyName,
                stringArray = {"*"}),
        @Policies.Item(
                key = sAllowlistPolicyName,
                stringArray = {sFooAllowlistFilter})
    })
    @OnlyRunIn(SINGLE_PROCESS) // http://crbug.com/660517
    public void testAllowlistedUrl() throws Throwable {
        navigateAndCheckOutcome(
                mFooTestUrl, /* startingErrorCount= */ 0, /* expectedErrorCount= */ 0);

        // Make sure it goes through the blocklist
        navigateAndCheckOutcome(
                mBarTestUrl, /* startingErrorCount= */ 0, /* expectedErrorCount= */ 1);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_CONNECT,
                mContentsClient.getOnReceivedErrorHelper().getError().errorCode);
    }

    // Tests that bad policy values are properly handled
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Policy"})
    @Policies.Add({
        @Policies.Item(key = sBlocklistPolicyName, string = "shouldBeAJsonArrayNotAString")
    })
    public void testBadPolicyValue() throws Exception {
        navigateAndCheckOutcome(
                mFooTestUrl, /* startingErrorCount= */ 0, /* expectedErrorCount= */ 0);
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
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
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

    private void setFilteringPolicy(
            final AwPolicyProvider testProvider,
            final String[] blocklistUrls,
            final String[] allowlistUrls) {
        final PolicyData[] policies = {
            new PolicyData.StrArray(sBlocklistPolicyName, blocklistUrls),
            new PolicyData.StrArray(sAllowlistPolicyName, allowlistUrls)
        };

        AbstractAppRestrictionsProvider.setTestRestrictions(
                PolicyData.asBundle(Arrays.asList(policies)));

        ThreadUtils.runOnUiThreadBlocking(() -> testProvider.refresh());

        // To avoid race conditions
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
