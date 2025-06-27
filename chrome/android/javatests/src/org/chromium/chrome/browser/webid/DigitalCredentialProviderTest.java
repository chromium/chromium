// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.app.Instrumentation.ActivityMonitor;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Promise;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Test suite for verifying the behavior of request digital credentials via the Web API. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class DigitalCredentialProviderTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/dc_mdocs.html";
    private static final String EXPECTED_MDOC = "test-mdoc";
    private static final String EXPECTED_CREATION_RESPONSE = "test-response";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private ActivityMonitor mActivityMonitor;
    private EmbeddedTestServer mTestServer;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock public IdentityCredentialsDelegate mDelegate;
    private WebPageStation mPage;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mPage = mActivityTestRule.startOnBlankPage();
        mTestServer = mActivityTestRule.getTestServer();
        DigitalIdentityProvider.setDelegateForTesting(mDelegate);
    }

    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testRequestMDoc() throws TimeoutException {
        when(mDelegate.get(any(), any(), any()))
                .thenAnswer(
                        input ->
                                Promise.fulfilled(
                                        new DigitalCredential(
                                                "protocol",
                                                ("{\"token\": \"" + EXPECTED_MDOC + "\"}")
                                                        .getBytes())));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "request_age_only_button");
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String mdoc =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        mActivityTestRule.getWebContents(),
                                        "document.getElementById('log').textContent");
                        String expected = "\"" + EXPECTED_MDOC + "\"";
                        Criteria.checkThat("mdoc string is not as expected.", mdoc, is(expected));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS_CREATION)
    public void testCreate() throws TimeoutException {
        when(mDelegate.create(any(), any(), any()))
                .thenAnswer(
                        input ->
                                Promise.fulfilled(
                                        new DigitalCredential(
                                                "protocol",
                                                ("{\"token\": \""
                                                        + EXPECTED_CREATION_RESPONSE
                                                        + "\"}"))));

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        DOMUtils.clickNode(mActivityTestRule.getWebContents(), "create_button");
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String response =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        mActivityTestRule.getWebContents(),
                                        "document.getElementById('log').textContent");
                        String expected = "\"" + EXPECTED_CREATION_RESPONSE + "\"";
                        Criteria.checkThat(
                                "Response string is not as expected.", response, is(expected));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }
}
