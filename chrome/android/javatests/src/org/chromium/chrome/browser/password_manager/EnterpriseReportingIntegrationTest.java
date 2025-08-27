// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assume.assumeTrue;

import androidx.test.filters.LargeTest;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.AndroidInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.policy.CloudManagementSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.enterprise.connectors.EnterpriseReportingEventType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.net.test.util.WebServer;

import java.util.concurrent.TimeoutException;

/** Test that password-related events are reported to an enterprise connector. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-chrome-browser-cloud-management",
})
// TODO(crbug.com/441339044): Re-enable the integration test for proto-based reporting.
@DisableFeatures("UploadRealtimeReportingEventsUsingProto")
@Batch(Batch.PER_CLASS)
public class EnterpriseReportingIntegrationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private TestWebServer mReportingServer;

    private static final String PASSWORD_FORM_URL =
            "/chrome/test/data/password/simple_password.html";
    private static final String USERNAME_FIELD_ID = "username_field";
    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String USERNAME_TEXT = "username@domain.com";
    private static final String PASSWORD_TEXT = "password";
    private static final String NEW_PASSWORD_TEXT = "new password";
    private static final String SUBMIT_BUTTON_ID = "input_submit_button";

    private static final String FAKE_GOOGLE_API_KEY = "fake-google-api-key";
    private static final String FAKE_DM_TOKEN = "fake-dm-token";

    private static final String REPORTING_ENDPOINT = "/?key=" + FAKE_GOOGLE_API_KEY;
    private static final String REPORTING_POLICY_NAME = "OnSecurityEventEnterpriseConnector";
    private static final String REPORTING_SUCCESS_HISTOGRAM =
            "Enterprise.ReportingEventUploadSuccess";
    private static final String REPORTING_FAILURE_HISTOGRAM =
            "Enterprise.ReportingEventUploadFailure";

    @Before
    public void setUp() throws Exception {
        mReportingServer = TestWebServer.start();
        // For simplicity, omit the `UploadEventsResponse` response, which the client doesn't look
        // at.
        mReportingServer.setEmptyResponse(REPORTING_ENDPOINT);

        // For authenticating to the fake reporting server.
        CloudManagementSharedPreferences.saveDmToken(FAKE_DM_TOKEN);

        CommandLine command_line = CommandLine.getInstance();
        command_line.appendSwitchWithValue("realtime-reporting-url", mReportingServer.getBaseUrl());
        command_line.appendSwitchWithValue("gaia-config-contents", buildGaiaConfig().toString());
        // Stop the browser from trying to talk to the real DM server. The command line will set the
        // policy needed, so a 404 will suffice.
        command_line.appendSwitchWithValue(
                "device-management-url", mReportingServer.getBaseUrl() + "does-not-exist");
    }

    @After
    public void tearDown() {
        mReportingServer.shutdown();
    }

    private JSONObject buildGaiaConfig() throws JSONException {
        var apiKeys = new JSONObject().put("GOOGLE_API_KEY", FAKE_GOOGLE_API_KEY);
        return new JSONObject().put("api_keys", apiKeys);
    }

    private JSONObject buildSecurityEventReportingPolicy(String eventName) throws JSONException {
        var eventDetails = new JSONObject().put("name", eventName).append("url_patterns", "*");
        return new JSONObject()
                .append("enabled_event_names", eventName)
                .append("enabled_opt_in_events", eventDetails)
                .put("service_provider", "google");
    }

    /** Build a histogram watcher that expects one successfully uploaded report and no failures. */
    private HistogramWatcher buildReportUploadWatcher(@EnterpriseReportingEventType int eventType) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(REPORTING_SUCCESS_HISTOGRAM, eventType)
                .expectNoRecords(REPORTING_FAILURE_HISTOGRAM)
                .build();
    }

    /** Parse the last security event report received, if any. */
    private JSONObject parseLastReport() throws JSONException {
        WebServer.HTTPRequest request = mReportingServer.getLastRequest(REPORTING_ENDPOINT);
        if (request == null) {
            return null;
        }
        var body = new String(request.getBody());
        return new JSONObject(body);
    }

    @Test
    @LargeTest
    public void testLoginEventReported() throws JSONException, TimeoutException {
        assumeTrue("Can set policy from command line", AndroidInfo.isDebugAndroid());

        var policyMap = new JSONObject();
        policyMap.append(REPORTING_POLICY_NAME, buildSecurityEventReportingPolicy("loginEvent"));
        CommandLine.getInstance().appendSwitchWithValue("policy", policyMap.toString());
        HistogramWatcher watcher =
                buildReportUploadWatcher(EnterpriseReportingEventType.LOGIN_EVENT);

        WebPageStation page = mActivityTestRule.startOnTestServerUrl(PASSWORD_FORM_URL);
        WebContents webContents = page.webContentsElement.value();
        DOMUtils.enterInputIntoTextField(webContents, USERNAME_FIELD_ID, USERNAME_TEXT);
        DOMUtils.enterInputIntoTextField(webContents, PASSWORD_NODE_ID, PASSWORD_TEXT);
        DOMUtils.clickNodeWithJavaScript(webContents, SUBMIT_BUTTON_ID);
        watcher.pollInstrumentationThreadUntilSatisfied();

        JSONObject report = parseLastReport();
        assertNotNull(report);
        assertEquals("Android", report.getJSONObject("device").getString("osPlatform"));
        var events = report.getJSONArray("events");
        assertEquals(1, events.length());
        var eventDetails = events.getJSONObject(0).getJSONObject("loginEvent");
        assertEquals(
                mActivityTestRule.getTestServer().getURL(PASSWORD_FORM_URL),
                eventDetails.getString("url"));
        // The username portion of the login will be masked, but the domain part shouldn't be.
        assertThat(eventDetails.getString("loginUserName"), endsWith("@domain.com"));
    }
}
