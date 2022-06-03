// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.util.Xml;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.xmlpull.v1.XmlSerializer;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.io.IOException;
import java.io.StringWriter;

/**
 * Unit tests for the Omaha ResponseParser.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ResponseParserTest {
    // Note that the Omaha server appends "/" to the end of the URL codebase.
    private static final String STRIPPED_URL =
            "https://play.google.com/store/apps/details?id=com.google.android.apps.chrome";
    private static final String URL = STRIPPED_URL + "/";
    private static final String NEXT_VERSION = "1.2.3.4";

    private static final String APP_STATUS_OK = "ok";
    private static final String APP_STATUS_RESTRICTED = "restricted";
    private static final String APP_STATUS_ERROR = "error-whatever-else";

    private static final String UPDATE_STATUS_OK = "ok";
    private static final String UPDATE_STATUS_NOUPDATE = "noupdate";
    private static final String UPDATE_STATUS_ERROR = "error-osnotsupported";
    private static final String UPDATE_STATUS_WTF = "omgwtfbbq";

    /**
     * Create XML for testing.
     * @param xmlProtocol Version number of the protocol.  Expected to be "3.0" for valid XML.
     * @param elapsedSeconds Number of seconds since server midnight.
     * @param appStatus Status to use for the <app> element.
     * @param addInstall Whether or not to add an install event.
     * @param addPing Whether or not to add a ping event.
     * @param updateStatus Status to use for the <updatecheck> element.
     * @return The completed XML.
     */
    private static String createTestXML(String xmlProtocol, String elapsedDays,
            String elapsedSeconds, String appStatus, boolean addInstall, boolean addPing,
            String updateStatus, String updateUrl) {
        StringWriter writer = new StringWriter();
        try {
            XmlSerializer serializer = Xml.newSerializer();
            serializer.setOutput(writer);
            serializer.startDocument("UTF-8", true);

            // Set up <response ...>
            serializer.startTag(null, "response");
            serializer.attribute(null, "server", "prod");
            if (xmlProtocol != null) {
                serializer.attribute(null, "protocol", xmlProtocol);
            }

            // Create <daystart> element.
            if (elapsedSeconds != null || elapsedDays != null) {
                serializer.startTag(null, "daystart");
                if (elapsedDays != null) {
                    serializer.attribute(null, "elapsed_days", elapsedDays);
                }
                if (elapsedSeconds != null) {
                    serializer.attribute(null, "elapsed_seconds", elapsedSeconds);
                }
                serializer.endTag(null, "daystart");
            }

            // Create <app> element with unused attribute.
            serializer.startTag(null, "app");
            serializer.attribute(null, "appid", "{APP_ID}");
            serializer.attribute(null, "status", appStatus);
            serializer.attribute(null, "unused", "attribute");

            if (addInstall) {
                serializer.startTag(null, "event");
                serializer.attribute(null, "status", "ok");
                serializer.endTag(null, "event");
            }

            if (addPing) {
                serializer.startTag(null, "ping");
                serializer.attribute(null, "status", "ok");
                serializer.endTag(null, "ping");
            }

            if (updateStatus != null) {
                serializer.startTag(null, "updatecheck");
                serializer.attribute(null, "status", updateStatus);
                if (UPDATE_STATUS_OK.equals(updateStatus)) {
                    createUpdateXML(serializer, updateUrl);
                }
                serializer.endTag(null, "updatecheck");
            }
            serializer.endTag(null, "app");

            // Create extraneous tag.
            serializer.startTag(null, "extraneous");
            serializer.attribute(null, "useless", "yes");
            serializer.endTag(null, "extraneous");

            serializer.endTag(null, "response");
            serializer.endDocument();
        } catch (IOException e) {
            Assert.fail("Caught an IOException creating the XML: " + e);
        } catch (IllegalArgumentException e) {
            Assert.fail("Caught an IllegalArgumentException creating the XML: " + e);
        } catch (IllegalStateException e) {
            Assert.fail("Caught an IllegalStateException creating the XML: " + e);
        }

        return writer.toString();
    }

    private static void createUpdateXML(XmlSerializer serializer, String updateUrl)
            throws IOException {
        // End result should look something like:
        // <updatecheck status="ok">
        //  <urls>
        //    <url codebase="URL" />
        //  </urls>
        //  <manifest garbage="attribute" version="NEXT_VERSION">
        //    <packages>
        //      <package hash="0" name="dummy.apk" required="true" size="0" />
        //    </packages>
        //    <actions>
        //      <action event="install" run="dummy.apk" />
        //      <action event="postinstall" />
        //    </actions>
        //  </manifest>
        //  <better be="ignored" />
        //</updatecheck>

        // Create <urls> and its descendants.
        serializer.startTag(null, "urls");
        if (updateUrl != null) {
            serializer.startTag(null, "url");
            serializer.attribute(null, "codebase", updateUrl);
            serializer.endTag(null, "url");
        }
        serializer.endTag(null, "urls");

        // Create <manifest> and its descendants.
        serializer.startTag(null, "manifest");
        serializer.attribute(null, "garbage", "attribute");
        serializer.attribute(null, "version", NEXT_VERSION);

        // Create <packages> and its children.
        serializer.startTag(null, "packages");
        serializer.startTag(null, "package");
        serializer.attribute(null, "hash", "0");
        serializer.attribute(null, "name", "dummy.apk");
        serializer.attribute(null, "required", "true");
        serializer.attribute(null, "size", "0");
        serializer.endTag(null, "package");
        serializer.endTag(null, "packages");

        // Create <actions> and its children.
        serializer.startTag(null, "actions");
        serializer.startTag(null, "action");
        serializer.attribute(null, "event", "install");
        serializer.attribute(null, "run", "dummy.apk");
        serializer.endTag(null, "action");

        serializer.startTag(null, "action");
        serializer.attribute(null, "event", "postinstall");
        serializer.endTag(null, "action");
        serializer.endTag(null, "actions");
        serializer.endTag(null, "manifest");

        // Create a dummy element for testing to make sure it's ignored.
        serializer.startTag(null, "dummy");
        serializer.attribute(null, "hopefully", "ignored");
        serializer.endTag(null, "dummy");
    }

    /**
     * Runs a test that is expected to pass.
     * @param appStatus Status to use for the <app> element.
     * @param addInstall Whether or not to add an install event.
     * @param addPing Whether or not to add a ping.
     * @param updateStatus Status to use for the <updatecheck> element.
     * @throws RequestFailureException Thrown if the test fails.
     */
    private static void runSuccessTest(String appStatus, boolean addInstall, boolean addPing,
            String updateStatus) throws RequestFailureException {
        String xml = createTestXML(
                "3.0", "4088", "12345", appStatus, addInstall, addPing, updateStatus, URL);
        ResponseParser parser =
                new ResponseParser(true, "{APP_ID}", addInstall, addPing, updateStatus != null);
        OmahaBase.VersionConfig versionConfig = parser.parseResponse(xml);

        Assert.assertEquals("elapsed_seconds doesn't match.", 12345, parser.getDaystartSeconds());
        Assert.assertEquals("elapsed_days doesn't match.", 4088, parser.getDaystartDays());
        Assert.assertEquals("<app> status doesn't match.", appStatus, parser.getAppStatus());
        Assert.assertEquals(
                "<updatecheck> status doesn't match.", updateStatus, parser.getUpdateStatus());
        if (UPDATE_STATUS_OK.equals(updateStatus)) {
            Assert.assertEquals(
                    "Version number doesn't match.", "1.2.3.4", parser.getNewVersion());
            Assert.assertEquals(
                    "Market URL doesn't match.", STRIPPED_URL, parser.getURL());
        } else {
            Assert.assertEquals(
                    "Version number doesn't match.", null, parser.getNewVersion());
            Assert.assertEquals(
                    "Market URL doesn't match.", null, parser.getURL());
        }
        Assert.assertEquals("Version number doesn't match.", versionConfig.latestVersion,
                parser.getNewVersion());
        Assert.assertEquals("URL doesn't match.", versionConfig.downloadUrl, parser.getURL());
    }

    /**
     * Runs a test that is expected to fail in a particular way.
     * @param xml XML to parse.
     * @param expectedErrorCode Expected error code.
     * @param expectInstall Whether or not the parser should expect an install event.
     * @param expectPing Whether or not the parser should expect a ping element.
     * @param expectUpdate Whether or not the parser should expect an update check.
     */
    private static void runFailureTest(String xml, int expectedErrorCode,
            boolean expectInstall, boolean expectPing, boolean expectUpdate) {
        ResponseParser parser =
                new ResponseParser(true, "{APP_ID}", expectInstall, expectPing, expectUpdate);

        try {
            parser.parseResponse(xml);
        } catch (RequestFailureException e) {
            Assert.assertEquals("Incorrect error code received.", expectedErrorCode, e.errorCode);
            return;
        }

        Assert.fail("Failed to throw RequestFailureException for bad XML.");
    }

    @Test
    @Feature({"Omaha"})
    public void testValidAllTypes() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, true, true, UPDATE_STATUS_OK);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidNoInstall() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, false, true, UPDATE_STATUS_OK);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidNoPing() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, true, false, UPDATE_STATUS_OK);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidNoUpdatecheck() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, true, true, null);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidUpdatecheckNoUpdate() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, false, false, UPDATE_STATUS_NOUPDATE);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidUpdatecheckError() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, false, false, UPDATE_STATUS_ERROR);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidUpdatecheckUnknown() throws RequestFailureException {
        runSuccessTest(APP_STATUS_OK, false, false, UPDATE_STATUS_WTF);
    }

    @Test
    @Feature({"Omaha"})
    public void testValidAppStatusRestricted() throws RequestFailureException {
        runSuccessTest(APP_STATUS_RESTRICTED, false, false, null);
    }

    @Test
    @Feature({"Omaha"})
    public void testFailBogusResponse() {
        String xml = "Bogus";
        runFailureTest(xml, RequestFailureException.ERROR_MALFORMED_XML, false, false, false);
    }

    @Test
    @Feature({"Omaha"})
    public void testBadResponseProtocol() {
        String xml = createTestXML(
                "2.0", "4088", "12345", APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_RESPONSE, false, false, false);
    }

    @Test
    @Feature({"Omaha"})
    public void testFailMissingDaystart() {
        String xml = createTestXML(
                "3.0", null, null, APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_DAYSTART, false, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testFailMissingDaystartSeconds() {
        String xml = createTestXML(
                "3.0", "4088", null, APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_DAYSTART, false, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testFailMissingDaystartDays() {
        String xml = createTestXML(
                "3.0", null, "12345", APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_DAYSTART, false, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagMissingUpdatecheck() {
        String xml = createTestXML("3.0", "4088", "12345", APP_STATUS_OK, true, false, null, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_UPDATECHECK, true, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagUnexpectedUpdatecheck() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, true, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_UPDATECHECK, true, false, false);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagMissingPing() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_PING, false, true, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagUnexpectedPing() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, false, true, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_PING, false, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagMissingInstall() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, false, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_EVENT, true, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagUnexpectedInstall() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, true, false, UPDATE_STATUS_OK, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_EVENT, false, false, true);
    }

    @Test
    @Feature({"Omaha"})
    public void testAppTagStatusError() {
        String xml =
                createTestXML("3.0", "4088", "12345", APP_STATUS_ERROR, false, false, null, URL);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_APP, false, false, false);
    }

    @Test
    @Feature({"Omaha"})
    public void testUpdatecheckMissingUrl() {
        String xml = createTestXML(
                "3.0", "4088", "12345", APP_STATUS_OK, false, false, UPDATE_STATUS_OK, null);
        runFailureTest(xml, RequestFailureException.ERROR_PARSE_URLS, false, false, true);
    }
}
