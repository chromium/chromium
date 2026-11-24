// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.webid.IdentityCredentialsDelegate.DigitalCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.transit.HtmlElement;
import org.chromium.content_public.browser.test.transit.WebPageFacility;
import org.chromium.content_public.browser.webid.DigitalIdentityUiWarningLevel;
import org.chromium.net.test.EmbeddedTestServer;

/** Test suite for verifying the behavior of request digital credentials via the Web API. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DigitalCredentialProviderTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/dc_mdocs.html";
    private static final String EXPECTED_MDOC = "test-mdoc";
    private static final String EXPECTED_CREATION_RESPONSE = "test-response";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private EmbeddedTestServer mTestServer;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock public IdentityCredentialsDelegate mDelegate;
    private WebPageStation mPage;

    private static class DigitalCredentialsPageFacility extends WebPageFacility {
        public final HtmlElement requestAgeOnlyButton =
                declareHtmlElement("#request_age_only_button");
        public final HtmlElement requestAgeAndNameButton =
                declareHtmlElement("#request_age_and_name_button");
        public final HtmlElement createButton = declareHtmlElement("#create_button");
        public final HtmlElement log = declareHtmlElement("#log");
    }

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
    public void testRequestMDoc() {
        when(mDelegate.get(any(), any(), any(), eq(DigitalIdentityUiWarningLevel.HINT_NO_ISSUES)))
                .thenAnswer(
                        input ->
                                Promise.fulfilled(
                                        new DigitalCredential(
                                                "protocol",
                                                "{\"token\": \"" + EXPECTED_MDOC + "\"}")));

        DigitalCredentialsPageFacility dcFacility = loadTestPage();
        dcFacility.requestAgeOnlyButton.clickTo().waitFor(dcFacility.log.textIs(EXPECTED_MDOC));
    }

    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS)
    public void testRequestMDocWithCaution() {
        when(mDelegate.get(any(), any(), any(), eq(DigitalIdentityUiWarningLevel.HINT_CAUTION)))
                .thenAnswer(
                        input ->
                                Promise.fulfilled(
                                        new DigitalCredential(
                                                "protocol",
                                                "{\"token\": \"" + EXPECTED_MDOC + "\"}")));

        DigitalCredentialsPageFacility dcFacility = loadTestPage();
        dcFacility.requestAgeAndNameButton.clickTo().waitFor(dcFacility.log.textIs(EXPECTED_MDOC));
    }

    @Test
    @LargeTest
    @EnableFeatures(ContentFeatureList.WEB_IDENTITY_DIGITAL_CREDENTIALS_CREATION)
    public void testCreate() {
        when(mDelegate.create(any(), any(), any()))
                .thenAnswer(
                        input ->
                                Promise.fulfilled(
                                        new DigitalCredential(
                                                "protocol",
                                                ("{\"token\": \""
                                                        + EXPECTED_CREATION_RESPONSE
                                                        + "\"}"))));

        DigitalCredentialsPageFacility dcFacility = loadTestPage();
        dcFacility
                .createButton
                .clickTo()
                .waitFor(dcFacility.log.textIs(EXPECTED_CREATION_RESPONSE));
    }

    private DigitalCredentialsPageFacility loadTestPage() {
        DigitalCredentialsPageFacility dcFacility = new DigitalCredentialsPageFacility();
        mPage =
                mPage.loadPageProgrammatically(
                        mTestServer.getURL(TEST_PAGE),
                        WebPageStation.newBuilder().withFacility(dcFacility));
        return dcFacility;
    }
}
