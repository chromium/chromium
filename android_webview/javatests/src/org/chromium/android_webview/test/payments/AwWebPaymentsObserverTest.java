// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.payments;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunnerWithParameters;
import org.chromium.android_webview.test.AwParameterizedTest;
import org.chromium.android_webview.test.AwSettingsMutation;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.net.test.util.TestWebServer;

/** Tests that WebView records 3D-Secure telemetry for form submissions. */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwWebPaymentsObserverTest extends AwParameterizedTest {
    private static final String CHALLENGE_REQUEST_HISTOGRAM =
            "Payments.ThreeDSecure.ChallengeRequest";
    private static final String CHALLENGE_RESPONSE_HISTOGRAM =
            "Payments.ThreeDSecure.ChallengeResponse";
    private static final String CHALLENGE_REQUEST_FIELD = "creq";
    private static final String CHALLENGE_RESPONSE_FIELD = "cres";
    private static final String MERCHANT_PAGE_PATH = "/merchant.html";
    private static final String CHALLENGE_FORM_PAGE_PATH = "/challenge_form.html";
    private static final String CHALLENGE_ENDPOINT_PATH = "/challenge";

    @Rule public AwActivityTestRule mRule;

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;

    public AwWebPaymentsObserverTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures(PaymentFeatureList.THREE_D_SECURE_TELEMETRY)
    public void testRecordsChallengeRequestOnFormSubmission() throws Throwable {
        HistogramWatcher challengeRequestHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        CHALLENGE_REQUEST_HISTOGRAM, /* value= */ true);

        submitChallengeFormInIframe(CHALLENGE_REQUEST_FIELD, "sample_challenge_request");

        challengeRequestHistogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @EnableFeatures(PaymentFeatureList.THREE_D_SECURE_TELEMETRY)
    public void testRecordsChallengeResponseOnFormSubmission() throws Throwable {
        HistogramWatcher challengeResponseHistogram =
                HistogramWatcher.newBuilder().expectAnyRecord(CHALLENGE_RESPONSE_HISTOGRAM).build();

        submitChallengeFormInIframe(CHALLENGE_RESPONSE_FIELD, "sample_challenge_response");

        challengeResponseHistogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisableFeatures(PaymentFeatureList.THREE_D_SECURE_TELEMETRY)
    public void testDoesNotRecordChallengeRequestWhenFeatureDisabled() throws Throwable {
        HistogramWatcher challengeRequestHistogram =
                HistogramWatcher.newBuilder().expectNoRecords(CHALLENGE_REQUEST_HISTOGRAM).build();

        submitChallengeFormInIframe(CHALLENGE_REQUEST_FIELD, "sample_challenge_request");
        // The navigation starts, and the observer would have recorded, before the request reaches
        // the server.
        AwActivityTestRule.pollInstrumentationThread(
                () -> mWebServer.getRequestCount(CHALLENGE_ENDPOINT_PATH) > 0);

        challengeRequestHistogram.assertExpected();
    }

    /**
     * Loads a merchant page with an iframe whose document submits a 3D-Secure challenge form to the
     * challenge endpoint on load.
     *
     * @param fieldName The name of the hidden form field, e.g. "creq" or "cres".
     * @param fieldValue The value of the hidden form field.
     */
    private void submitChallengeFormInIframe(String fieldName, String fieldValue) {
        String challengeEndpointUrl =
                mWebServer.setResponse(
                        CHALLENGE_ENDPOINT_PATH,
                        "<html><body>challenge</body></html>",
                        /* responseHeaders= */ null);
        String challengeFormPageUrl =
                mWebServer.setResponse(
                        CHALLENGE_FORM_PAGE_PATH,
                        "<html><body onload=\"document.forms[0].submit();\">"
                                + "<form method=\"POST\" action=\""
                                + challengeEndpointUrl
                                + "\">"
                                + "<input type=\"hidden\" name=\""
                                + fieldName
                                + "\" value=\""
                                + fieldValue
                                + "\">"
                                + "</form></body></html>",
                        /* responseHeaders= */ null);
        String merchantPageUrl =
                mWebServer.setResponse(
                        MERCHANT_PAGE_PATH,
                        "<html><body><iframe src=\""
                                + challengeFormPageUrl
                                + "\"></iframe></body></html>",
                        /* responseHeaders= */ null);
        mRule.loadUrlAsync(mTestContainerView.getAwContents(), merchantPageUrl);
    }
}
