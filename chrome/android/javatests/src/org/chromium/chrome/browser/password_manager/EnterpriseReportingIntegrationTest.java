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

import com.google.protobuf.CodedInputStream;
import com.google.protobuf.WireFormat;

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
import org.chromium.base.test.util.DisableIf;
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
import org.chromium.ui.base.DeviceFormFactor;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Test that password-related events are reported to an enterprise connector. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-chrome-browser-cloud-management",
    "policy=" + EnterpriseReportingIntegrationTest.REPORTING_POLICY_STRING,
})
@DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP) // crbug.com/463649037
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
    private static final String SUBMIT_BUTTON_ID = "input_submit_button";

    private static final String FAKE_GOOGLE_API_KEY = "fake-google-api-key";
    private static final String FAKE_DM_TOKEN = "fake-dm-token";

    private static final String REPORTING_ENDPOINT = "/?key=" + FAKE_GOOGLE_API_KEY;
    public static final String REPORTING_POLICY_STRING =
            "{\"OnSecurityEventEnterpriseConnector\":[{\"enabled_event_names\":[\"loginEvent\"],"
                + "\"enabled_opt_in_events\":[{\"name\":\"loginEvent\",\"url_patterns\":[\"*\"]}],"
                + "\"service_provider\":\"google\"}]}";

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

        CommandLine commandLine = CommandLine.getInstance();
        commandLine.appendSwitchWithValue("realtime-reporting-url", mReportingServer.getBaseUrl());
        commandLine.appendSwitchWithValue("gaia-config-contents", buildGaiaConfig().toString());
        // Stop the browser from trying to talk to the real DM server. The command line will set the
        // policy needed, so a 404 will suffice.
        commandLine.appendSwitchWithValue(
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

    /** Build a histogram watcher that expects one successfully uploaded report and no failures. */
    private HistogramWatcher buildReportUploadWatcher(@EnterpriseReportingEventType int eventType) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(REPORTING_SUCCESS_HISTOGRAM, eventType)
                .expectNoRecords(REPORTING_FAILURE_HISTOGRAM)
                .build();
    }

    /** Get the last security event report received, if any. */
    private SimpleProto getLastReport() {
        WebServer.HTTPRequest request = mReportingServer.getLastRequest(REPORTING_ENDPOINT);
        if (request == null) {
            return null;
        }
        return new SimpleProto(request.getBody());
    }

    @Test
    @LargeTest
    public void testLoginEventReported() throws TimeoutException, IOException {
        assumeTrue("Can set policy from command line", AndroidInfo.isDebugAndroid());

        HistogramWatcher watcher =
                buildReportUploadWatcher(EnterpriseReportingEventType.LOGIN_EVENT);

        WebPageStation page = mActivityTestRule.startOnTestServerUrl(PASSWORD_FORM_URL);
        WebContents webContents = page.webContentsElement.value();
        DOMUtils.enterInputIntoTextField(webContents, USERNAME_FIELD_ID, USERNAME_TEXT);
        DOMUtils.enterInputIntoTextField(webContents, PASSWORD_NODE_ID, PASSWORD_TEXT);
        DOMUtils.clickNodeWithJavaScript(webContents, SUBMIT_BUTTON_ID);
        watcher.pollInstrumentationThreadUntilSatisfied();

        SimpleProto report = getLastReport();
        assertNotNull(report);

        // Android build targets do not include full proto libraries.
        // See proto definitions at:
        // UploadEventsRequest: components/enterprise/common/proto/upload_request_response.proto
        // Device, Event:
        // components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.proto
        // LoginEvent: components/enterprise/common/proto/synced/browser_events.proto
        assertEquals(
                "Android",
                report.getFieldByTag(/*UploadEventsRequest.device*/ 4)
                        .getFieldByTag(/*Device.os_platform*/ 4)
                        .asString());

        List<SimpleProto> events = report.getRepeatedFieldByTag(/*UploadEventsRequest.events*/ 3);
        assertEquals(1, events.size());

        SimpleProto eventDetails = events.get(0).getFieldByTag(/*Event.login_event*/ 111);

        assertEquals(
                mActivityTestRule.getTestServer().getURL(PASSWORD_FORM_URL),
                eventDetails.getFieldByTag(/*LoginEvent.url*/ 1).asString());

        assertThat(
                eventDetails.getFieldByTag(/*LoginEvent.login_user_name*/ 5).asString(),
                endsWith("@domain.com"));
    }

    /** A helper class to parse protos by tag number. */
    static class SimpleProto {
        private final byte[] mRawBytes;

        SimpleProto(byte[] rawBytes) {
            this.mRawBytes = rawBytes;
        }

        SimpleProto getFieldByTag(int tagNumber) throws IOException {
            CodedInputStream input = CodedInputStream.newInstance(mRawBytes);
            int tag;

            while ((tag = input.readTag()) != 0) {
                int currentTagNumber = WireFormat.getTagFieldNumber(tag);

                if (currentTagNumber == tagNumber) {
                    if (WireFormat.getTagWireType(tag) == WireFormat.WIRETYPE_LENGTH_DELIMITED) {
                        return new SimpleProto(input.readByteArray());
                    } else {
                        throw new IllegalStateException(
                                "Tag "
                                        + tagNumber
                                        + " is not a length-delimited field (nested"
                                        + " proto/string).");
                    }
                } else {
                    input.skipField(tag);
                }
            }
            return null;
        }

        List<SimpleProto> getRepeatedFieldByTag(int tagNumber) throws IOException {
            List<SimpleProto> results = new ArrayList<>();
            CodedInputStream input = CodedInputStream.newInstance(mRawBytes);
            int tag;

            while ((tag = input.readTag()) != 0) {
                int currentTagNumber = WireFormat.getTagFieldNumber(tag);

                if (currentTagNumber == tagNumber) {
                    if (WireFormat.getTagWireType(tag) == WireFormat.WIRETYPE_LENGTH_DELIMITED) {
                        results.add(new SimpleProto(input.readByteArray()));
                    } else {
                        throw new IllegalStateException(
                                "Tag " + tagNumber + " is not a length-delimited field.");
                    }
                } else {
                    input.skipField(tag);
                }
            }
            return results;
        }

        String asString() {
            return new String(mRawBytes, StandardCharsets.UTF_8);
        }
    }
}
