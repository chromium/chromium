// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.SettableFuture;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.client_hints.AwUserAgentMetadata;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test suite for user-agent client hints.
 * Notes: When verifying sec-ch-ua-mobile client hints value on WebView tests, we can't assume
 * mobile is always true because there is some test bots don't set to use mobile user-agent.
 */
@DoNotBatch(reason = "These tests conflict with each other.")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientHintsTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String[] USER_AGENT_CLIENT_HINTS = {
        "sec-ch-ua",
        "sec-ch-ua-arch",
        "sec-ch-ua-platform",
        "sec-ch-ua-model",
        "sec-ch-ua-mobile",
        "sec-ch-ua-full-version",
        "sec-ch-ua-platform-version",
        "sec-ch-ua-bitness",
        "sec-ch-ua-full-version-list",
        "sec-ch-ua-wow64",
        "sec-ch-ua-form-factors"
    };

    private static final String ANDROID_WEBVIEW_BRAND_NAME = "Android WebView";

    private static final String CHROME_PRODUCT_PATTERN = "Chrome/(\\d+).(\\d+).(\\d+).(\\d+)";

    private static final String WEBVIEW_REDUCED_UA_PATTERN =
            "Mozilla/5\\.0 \\((.+)\\) AppleWebKit\\/537\\.36 \\(KHTML, like Gecko\\) Version/4\\.0"
                    + " Chrome/(\\d+)\\.0\\.0\\.0( Mobile)? Safari/537\\.36";

    private static class ClientHintsTestResult {
        public Map<String, String> mHttpHeaderClientHints;
        public JSONObject mJsClientHints;

        public ClientHintsTestResult(
                Map<String, String> httpHeaderClientHints, JSONObject jsClientHints) {
            this.mHttpHeaderClientHints = httpHeaderClientHints;
            this.mJsClientHints = jsClientHints;
        }
    }

    public ClientHintsTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testClientHintsDefault() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);

        // First round uses insecure server.
        AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());
        verifyClientHintBehavior(server, contents, contentsClient, false);
        clearCookies();
        server.stopAndDestroyServer();

        // Second round uses secure server.
        server =
                AwEmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        ServerCertificate.CERT_OK);
        verifyClientHintBehavior(server, contents, contentsClient, true);
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsPrefersReducedTransparency",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testAllClientHints() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Please keep these here (and below) in the same order as web_client_hints_types.mojom.
        final String[] activeClientHints = {
            "device-memory",
            "dpr",
            "width",
            "viewport-width",
            "rtt",
            "downlink",
            "ect",
            // "sec-ch-lang" was removed in M96
            "sec-ch-ua",
            "sec-ch-ua-arch",
            "sec-ch-ua-platform",
            "sec-ch-ua-model",
            "sec-ch-ua-mobile",
            "sec-ch-ua-full-version",
            "sec-ch-ua-platform-version",
            "sec-ch-prefers-color-scheme",
            "sec-ch-ua-bitness",
            "sec-ch-viewport-height",
            "sec-ch-device-memory",
            "sec-ch-dpr",
            "sec-ch-width",
            "sec-ch-viewport-width",
            "sec-ch-ua-full-version-list",
            "sec-ch-ua-wow64",
            "save-data",
            "sec-ch-prefers-reduced-motion",
            "sec-ch-ua-form-factors",
            "sec-ch-prefers-reduced-transparency",
            // Add client hints above. The final row should have a trailing comma for cleaner
            // diffs.
        };
        final String url =
                server.getURL(
                        "/client-hints-header?accept-ch=" + String.join(",", activeClientHints));

        // Load twice to be sure hints are returned, then parse the results.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        String textContent =
                mActivityTestRule
                        .getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");

        // Get client hints from HTTP request header.
        HashMap<String, String> clientHintsMap = getClientHints(textContent);

        // If you're here because this line broke, please update this test to verify whichever
        // client hints were added or removed by changing `activeClientHints` above.
        Assert.assertEquals(
                "The number of client hints is unexpected. If you intentionally added "
                        + "or removed a client hint, please update this test.",
                activeClientHints.length,
                clientHintsMap.size());

        // All client hints must be verified for default behavior.
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("device-memory")) > 0);
        Assert.assertTrue(Double.valueOf(clientHintsMap.get("dpr")) > 0);
        // This is only set for subresources.
        Assert.assertEquals("HEADER_NOT_FOUND", clientHintsMap.get("width"));
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("viewport-width")) > 0);
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("rtt")) == 0);
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("downlink")) == 0);
        // This is the holdback value (the default in some cases).
        Assert.assertEquals("4g", clientHintsMap.get("ect"));
        // This client hint was removed.
        Assert.assertNull(clientHintsMap.get("sec-ch-lang"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-arch"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-platform"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-model"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-mobile"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-full-version"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals(
                "HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-platform-version"));
        Assert.assertEquals("light", clientHintsMap.get("sec-ch-prefers-color-scheme"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-bitness"));
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("sec-ch-viewport-height")) > 0);
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("sec-ch-device-memory")) > 0);
        Assert.assertTrue(Double.valueOf(clientHintsMap.get("sec-ch-dpr")) > 0);
        // This is only set for subresources.
        Assert.assertEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-width"));
        Assert.assertTrue(Integer.valueOf(clientHintsMap.get("sec-ch-viewport-width")) > 0);
        // User agent client hints are active on android webview.
        Assert.assertNotEquals(
                "HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-full-version-list"));
        // User agent client hints are active on android webview.
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-wow64"));
        // This client hint isn't sent when data-saver is off.
        Assert.assertEquals("HEADER_NOT_FOUND", clientHintsMap.get("save-data"));
        Assert.assertNotEquals(
                "HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-prefers-reduced-motion"));
        Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-form-factors"));
        Assert.assertNotEquals(
                "HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-prefers-reduced-transparency"));

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testEnableUserAgentClientHintsNoCustom() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ null,
                /* expectHighEntropyClientHints= */ true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testEnableUserAgentClientHintsCustomOverride() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ "CustomUserAgentOverride",
                /* expectHighEntropyClientHints= */ false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testEnableUserAgentClientHintsModifyDefaultUserAgent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
        String defaultUserAgent = settings.getUserAgentString();

        // Override user-agent with appending suffix.
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ defaultUserAgent + "CustomUserAgentSuffix",
                /* expectHighEntropyClientHints= */ true);

        // Override user-agent with adding prefix.
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ "CustomUserAgentPrefix" + defaultUserAgent,
                /* expectHighEntropyClientHints= */ true);

        // Override user-agent with adding both prefix and suffix.
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ "CustomUserAgentPrefix"
                        + defaultUserAgent
                        + "CustomUserAgentSuffix",
                /* expectHighEntropyClientHints= */ true);

        // Override user-agent with empty string, it's assumed to use system default.
        verifyUserAgentOverrideClientHints(
                /* contentsClient= */ contentsClient,
                /* contents= */ contents,
                /* customUserAgent= */ "",
                /* expectHighEntropyClientHints= */ true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    public void testEnableUserAgentClientHintsJavaScript() throws Throwable {
        verifyClientHintsJavaScript(/* useCustomUserAgent= */ false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testEnableUserAgentClientHintsOverrideJavaScript() throws Throwable {
        verifyClientHintsJavaScript(/* useCustomUserAgent= */ true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testCriticalClientHints() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        // First we verify that sec-ch-device-memory (critical) is returned on the first load.
        String url =
                server.getURL(
                        "/critical-client-hints-header?accept-ch=sec-ch-device-memory&"
                                + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", false);

        // Second we verify that device-memory (not critical) won't cause a reload.
        url =
                server.getURL(
                        "/critical-client-hints-header?accept-ch=sec-ch-device-memory,device-memory&"
                            + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", false);

        // Third we verify that device-memory is returned on the final load even with no request.
        url =
                server.getURL(
                        "/critical-client-hints-header?accept-ch=sec-ch-device-memory&"
                                + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", true);

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataGetApi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);

        Map<String, Object> defaultUserAgentMetadata = settings.getUserAgentMetadataMap();
        // Override part of value in user-agent metadata.
        settings.setUserAgentMetadataFromMap(
                Map.of(AwUserAgentMetadata.MetadataKeys.PLATFORM, "fake_platform"));

        // Verify getUserAgentMetadataMap API returns the correct value.
        Map<String, Object> customUserAgentMetadata = settings.getUserAgentMetadataMap();
        Assert.assertEquals(
                "Android", defaultUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.PLATFORM));
        Assert.assertEquals(
                "fake_platform",
                customUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.PLATFORM));

        // Verify the remaining of entries are equals.
        defaultUserAgentMetadata.remove(AwUserAgentMetadata.MetadataKeys.PLATFORM);
        customUserAgentMetadata.remove(AwUserAgentMetadata.MetadataKeys.PLATFORM);
        Assert.assertEquals(defaultUserAgentMetadata, customUserAgentMetadata);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataInvalidBitness() throws Throwable {
        try {
            getClientHintsWithOverrides(
                    Map.of(AwUserAgentMetadata.MetadataKeys.BITNESS, "foo"),
                    /* overrideUserAgent= */ null);
            Assert.fail("Should have thrown exception.");
        } catch (IllegalArgumentException e) {
            Assert.assertEquals(
                    "AwUserAgentMetadata map does not have right type of "
                            + "value for key: BITNESS",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataDefaultBitness() throws Throwable {
        // Override with bitness 0, we expect it return an empty string.
        ClientHintsTestResult clientHintsResult =
                getClientHintsWithOverrides(
                        Map.of(AwUserAgentMetadata.MetadataKeys.BITNESS, 0),
                        /* overrideUserAgent= */ null);

        // Verify Http header client hints results.
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals("\"\"", clientHintsMap.get("sec-ch-ua-bitness"));
        Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));

        // Verify js client hints results.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals("", jsClientHints.getString("bitness"));
        Assert.assertEquals("Android", jsClientHints.getString("platform"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataValidBitness() throws Throwable {
        ClientHintsTestResult clientHintsResult =
                getClientHintsWithOverrides(
                        Map.of(AwUserAgentMetadata.MetadataKeys.BITNESS, 32),
                        /* overrideUserAgent= */ null);

        // Verify Http header client hints results.
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals("\"32\"", clientHintsMap.get("sec-ch-ua-bitness"));

        // Verify js client hints results.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals("32", jsClientHints.getString("bitness"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataOverrideBrand() throws Throwable {
        // override with empty full version
        ClientHintsTestResult clientHintsResult =
                getClientHintsWithOverrides(
                        Map.of(
                                AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                                new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", ""}}),
                        /* overrideUserAgent= */ null);

        // Verify Http header client hints results.
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals(
                "\"brand1\";v=\"1\", \"brand2\";v=\"2\"", clientHintsMap.get("sec-ch-ua"));
        Assert.assertEquals(
                "\"brand1\";v=\"1.1.1\"", clientHintsMap.get("sec-ch-ua-full-version-list"));
        Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));

        // Verify js client hints results.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2\"}]",
                jsClientHints.getString("brands"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1.1.1\"}]",
                jsClientHints.getString("fullVersionList"));
        Assert.assertEquals("Android", jsClientHints.getString("platform"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataInvalidBrand() throws Throwable {
        // Test invalid input brand array: size only 2.
        try {
            getClientHintsWithOverrides(
                    Map.of(
                            AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                            new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2"}}),
                    /* overrideUserAgent= */ null);
            Assert.fail("Should have thrown exception.");
        } catch (IllegalArgumentException e) {
            Assert.assertEquals(
                    "AwUserAgentMetadata map does not have right type of value "
                            + "for key: "
                            + AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST
                            + ", expect brand item length:3, actual:2",
                    e.getMessage());
        }

        // Test invalid input brand array with null.
        try {
            getClientHintsWithOverrides(
                    Map.of(
                            AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                            new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", null}}),
                    /* overrideUserAgent= */ null);
            Assert.fail("Should have thrown exception.");
        } catch (IllegalArgumentException e) {
            Assert.assertEquals(
                    "AwUserAgentMetadata map does not have right type of value "
                            + "for key: "
                            + AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST
                            + ", brand item should not set as null",
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataClearOverride() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);

        // 1. Override platform, brands, and wow64
        settings.setUserAgentMetadataFromMap(
                Map.of(
                        AwUserAgentMetadata.MetadataKeys.PLATFORM,
                        "fake_platform",
                        AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                        new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", "2.2.2"}},
                        AwUserAgentMetadata.MetadataKeys.WOW64,
                        true));

        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Make first request and verify client hints.
        ClientHintsTestResult clientHintsResult =
                makeRequestAndGetClientHints(server, contents, contentsClient);
        // Verify Http header client hints results.
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals("\"fake_platform\"", clientHintsMap.get("sec-ch-ua-platform"));
        Assert.assertEquals(
                "\"brand1\";v=\"1\", \"brand2\";v=\"2\"", clientHintsMap.get("sec-ch-ua"));
        Assert.assertEquals(
                "\"brand1\";v=\"1.1.1\", \"brand2\";v=\"2.2.2\"",
                clientHintsMap.get("sec-ch-ua-full-version-list"));
        Assert.assertEquals("?1", clientHintsMap.get("sec-ch-ua-wow64"));

        // Verify js client hints results.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals("fake_platform", jsClientHints.getString("platform"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2\"}]",
                jsClientHints.getString("brands"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1.1.1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2.2.2\"}]",
                jsClientHints.getString("fullVersionList"));
        Assert.assertTrue(jsClientHints.getBoolean("wow64"));

        // 2. Reset previous overrides for platform.
        HashMap<String, Object> overrideResetMap = new HashMap<>();
        overrideResetMap.put(AwUserAgentMetadata.MetadataKeys.PLATFORM, null);
        overrideResetMap.put(
                AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                new String[][] {{"brand1", "2", "2.1.1"}, {"brand2", "3", "3.2.2"}});
        overrideResetMap.put(AwUserAgentMetadata.MetadataKeys.BITNESS, 100);
        settings.setUserAgentMetadataFromMap(overrideResetMap);

        // Make second request and verify clear overrides result.
        clientHintsResult = makeRequestAndGetClientHints(server, contents, contentsClient);

        // Verify Http header client hints results.
        clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        // Platform should be reset as default.
        Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));
        // Brand should use the latest override value.
        Assert.assertEquals(
                "\"brand1\";v=\"2\", \"brand2\";v=\"3\"", clientHintsMap.get("sec-ch-ua"));
        Assert.assertEquals(
                "\"brand1\";v=\"2.1.1\", \"brand2\";v=\"3.2.2\"",
                clientHintsMap.get("sec-ch-ua-full-version-list"));
        // Wow64 has been reset to default.
        Assert.assertEquals("?0", clientHintsMap.get("sec-ch-ua-wow64"));
        // Bitness should use the latest override value.
        Assert.assertEquals("\"100\"", clientHintsMap.get("sec-ch-ua-bitness"));

        // Verify js client hints results.
        jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals("Android", jsClientHints.getString("platform"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"2\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"3\"}]",
                jsClientHints.getString("brands"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"2.1.1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"3.2.2\"}]",
                jsClientHints.getString("fullVersionList"));
        Assert.assertFalse(jsClientHints.getBoolean("wow64"));
        Assert.assertEquals("100", jsClientHints.getString("bitness"));

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataClearOverrideWithCustomUA() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);

        // 1. Override user-agent metadata and overridden user-agent doesn't contains default
        // user-agent.
        settings.setUserAgentMetadataFromMap(
                Map.of(
                        AwUserAgentMetadata.MetadataKeys.PLATFORM,
                        "fake_platform",
                        AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                        new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", "2.2.2"}},
                        AwUserAgentMetadata.MetadataKeys.WOW64,
                        true));
        settings.setUserAgentString("testCustomUserAgent");

        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Make first request and verify client hints.
        ClientHintsTestResult clientHintsResult =
                makeRequestAndGetClientHints(server, contents, contentsClient);
        // Verify Http header client hints results.
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals("\"fake_platform\"", clientHintsMap.get("sec-ch-ua-platform"));
        Assert.assertEquals(
                "\"brand1\";v=\"1\", \"brand2\";v=\"2\"", clientHintsMap.get("sec-ch-ua"));
        Assert.assertEquals(
                "\"brand1\";v=\"1.1.1\", \"brand2\";v=\"2.2.2\"",
                clientHintsMap.get("sec-ch-ua-full-version-list"));
        Assert.assertEquals("?1", clientHintsMap.get("sec-ch-ua-wow64"));

        // Verify js client hints results.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals("fake_platform", jsClientHints.getString("platform"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2\"}]",
                jsClientHints.getString("brands"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1.1.1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2.2.2\"}]",
                jsClientHints.getString("fullVersionList"));
        Assert.assertTrue(jsClientHints.getBoolean("wow64"));

        // 2. Clear previous overrides for platform, and make second request and verify clear
        // overrides result.
        settings.setUserAgentMetadataFromMap(null);
        clientHintsResult = makeRequestAndGetClientHints(server, contents, contentsClient);

        // Verify Http header client hints results only generate system default low-entropy client
        // hints, high-entropy client hints are empty.
        clientHintsMap = clientHintsResult.mHttpHeaderClientHints;
        Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));
        Assert.assertTrue(
                clientHintsMap.get("sec-ch-ua").indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
        Assert.assertTrue(clientHintsMap.get("sec-ch-ua-full-version-list").isEmpty());

        // Verify js client hints results only generate system default low-entropy client
        // hints, high-entropy client hints are empty.
        jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertFalse(jsClientHints.getString("brands").isEmpty());
        Assert.assertTrue(
                jsClientHints.getString("brands").indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
        Assert.assertFalse(jsClientHints.getString("mobile").isEmpty());
        Assert.assertEquals("Android", jsClientHints.getString("platform"));
        Assert.assertEquals("[]", jsClientHints.getString("fullVersionList"));

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataClearOverrideVerifyGetApi() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);

        // Override platform in user-agent metadata.
        String[][] overrideBrands =
                new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", "2.2.2"}};
        settings.setUserAgentMetadataFromMap(
                Map.of(
                        AwUserAgentMetadata.MetadataKeys.PLATFORM,
                        "fake_platform",
                        AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                        overrideBrands,
                        AwUserAgentMetadata.MetadataKeys.WOW64,
                        true));
        Map<String, Object> customUserAgentMetadata = settings.getUserAgentMetadataMap();
        Assert.assertEquals(
                "fake_platform",
                customUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.PLATFORM));
        Assert.assertEquals(
                true, customUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.WOW64));
        Assert.assertEquals(
                Arrays.deepToString(overrideBrands),
                Arrays.deepToString(
                        (String[][])
                                customUserAgentMetadata.get(
                                        AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST)));
        // Update the outside brand, the deep copy brand version list shouldn't change.
        overrideBrands[0][0] = "updated_brand";
        Assert.assertNotEquals(
                Arrays.deepToString(overrideBrands),
                Arrays.deepToString(
                        (String[][])
                                customUserAgentMetadata.get(
                                        AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST)));

        // Reset the previous override, the user-agent metadata should be the default value.
        HashMap<String, Object> overrideResetMap = new HashMap<>();
        overrideResetMap.put(AwUserAgentMetadata.MetadataKeys.PLATFORM, null);
        settings.setUserAgentMetadataFromMap(overrideResetMap);
        customUserAgentMetadata = settings.getUserAgentMetadataMap();
        Assert.assertEquals(
                "Android", customUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.PLATFORM));
        Assert.assertEquals(
                false, customUserAgentMetadata.get(AwUserAgentMetadata.MetadataKeys.WOW64));

        String[][] actualOverrideBrands =
                (String[][])
                        customUserAgentMetadata.get(
                                AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST);
        List<String> brands = new ArrayList<>();
        for (String[] bv : actualOverrideBrands) {
            brands.add(bv[0]);
        }
        Assert.assertTrue(brands.contains("Android WebView"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataFullWithoutUAOverrides() throws Throwable {
        // Test overriding full set of user-agent metadata and has no user-agent overrides.
        verifyOverrideUaAndOverrideUaMetadata(null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataFullWithCustomUa() throws Throwable {
        // Test overriding full set of user-agent metadata and overriding user-agent doesn't
        // contains default user-agent.
        verifyOverrideUaAndOverrideUaMetadata("customUserAgent");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataFullWithDefaultUA() throws Throwable {
        // Test overriding full set of user-agent metadata and overriding user-agent contains
        // default user-agent.
        verifyOverrideUaAndOverrideUaMetadata(getDefaultUserAgent() + "_Suffix");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({
        "enable-features=ClientHintsFormFactors",
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
    })
    public void testOverrideUserAgentMetadataNullWithCustomUserAgent() throws Throwable {
        // High-entropy client hints should not be populated when overridden user-agent
        // doesn't contain default ua, and users also don't override user-agent metadata.
        ClientHintsTestResult clientHintsResult =
                getClientHintsWithOverrides(
                        /* uaMetadataOverrides= */ null,
                        /* overrideUserAgent= */ "customUserAgent");
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;

        // Verify http header low-entropy client hints result.
        Assert.assertFalse(clientHintsMap.get("sec-ch-ua").isEmpty());
        Assert.assertFalse(clientHintsMap.get("sec-ch-ua-mobile").isEmpty());
        Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));
        // Verify http header high-entropy client hints result, here we take some client hints
        // should not be empty if generated as examples to verify.
        Assert.assertEquals("\"\"", clientHintsMap.get("sec-ch-ua-full-version"));
        Assert.assertEquals("", clientHintsMap.get("sec-ch-ua-full-version-list"));
        Assert.assertEquals("\"\"", clientHintsMap.get("sec-ch-ua-platform-version"));

        // Verify js low-entropy client hints result.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertFalse(jsClientHints.getString("brands").isEmpty());
        Assert.assertFalse(jsClientHints.getString("mobile").isEmpty());
        Assert.assertEquals("Android", jsClientHints.getString("platform"));

        // Verify js high-entropy client hints result.
        Assert.assertEquals("[]", jsClientHints.getString("fullVersionList"));
        Assert.assertEquals("", jsClientHints.getString("uaFullVersion"));
        Assert.assertEquals("", jsClientHints.getString("platformVersion"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    public void testDefaultUserAgentDefaultReductionOverride() throws Throwable {
        String defaultUserAgent = getDefaultUserAgent();
        // Verify user-agent minor version not reduced.
        Matcher uaMatcher = Pattern.compile(CHROME_PRODUCT_PATTERN).matcher(defaultUserAgent);
        Assert.assertTrue(uaMatcher.find());
        Assert.assertNotEquals(
                "0.0.0",
                String.format(
                        "%s.%s.%s", uaMatcher.group(2), uaMatcher.group(3), uaMatcher.group(4)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    @CommandLineFlags.Add({"enable-features=ReduceUserAgentMinorVersion"})
    public void testDefaultUserAgentEnableReductionOverride() throws Throwable {
        String defaultUserAgent = getDefaultUserAgent();
        // Verify user-agent minor version is reduced.
        Matcher uaMatcher = Pattern.compile(CHROME_PRODUCT_PATTERN).matcher(defaultUserAgent);
        Assert.assertTrue(uaMatcher.find());
        Assert.assertEquals(
                "0.0.0",
                String.format(
                        "%s.%s.%s", uaMatcher.group(2), uaMatcher.group(3), uaMatcher.group(4)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUserAgentString()")
    @CommandLineFlags.Add({
        "enable-features=ReduceUserAgentMinorVersion,WebViewReduceUAAndroidVersionDeviceModel"
    })
    public void testDefaultUserAgentEnableAllReduction() throws Throwable {
        String defaultUserAgent = getDefaultUserAgent();
        // Verify user-agent is reduced.
        Matcher uaMatcher = Pattern.compile(WEBVIEW_REDUCED_UA_PATTERN).matcher(defaultUserAgent);
        Assert.assertTrue(uaMatcher.find());
        Assert.assertEquals("Linux; Android 10; K; wv", uaMatcher.group(1));
    }

    private void verifyOverrideUaAndOverrideUaMetadata(String overrideUserAgent) throws Throwable {
        ClientHintsTestResult clientHintsResult =
                getClientHintsWithOverrides(
                        makeFakeMetadata(), /* overrideUserAgent= */ overrideUserAgent);
        Map<String, String> clientHintsMap = clientHintsResult.mHttpHeaderClientHints;

        // Verify http header client hints result.
        Assert.assertEquals(
                "\"brand1\";v=\"1\", \"brand2\";v=\"2\"", clientHintsMap.get("sec-ch-ua"));
        Assert.assertEquals(
                "\"brand1\";v=\"1.1.1\", \"brand2\";v=\"2.2.2\"",
                clientHintsMap.get("sec-ch-ua-full-version-list"));
        Assert.assertEquals("\"2.2.2\"", clientHintsMap.get("sec-ch-ua-full-version"));
        Assert.assertEquals("\"overrideTest\"", clientHintsMap.get("sec-ch-ua-platform"));
        Assert.assertEquals("\"1.2.3\"", clientHintsMap.get("sec-ch-ua-platform-version"));
        Assert.assertEquals("\"x86_123\"", clientHintsMap.get("sec-ch-ua-arch"));
        Assert.assertEquals("\"foo_model\"", clientHintsMap.get("sec-ch-ua-model"));
        Assert.assertEquals("?1", clientHintsMap.get("sec-ch-ua-mobile"));
        Assert.assertEquals("\"128\"", clientHintsMap.get("sec-ch-ua-bitness"));
        Assert.assertEquals("?1", clientHintsMap.get("sec-ch-ua-wow64"));
        Assert.assertEquals(
                "\"Automotive\", \"Tablet\"", clientHintsMap.get("sec-ch-ua-form-factors"));

        // Verify js client hints result.
        JSONObject jsClientHints = clientHintsResult.mJsClientHints;
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2\"}]",
                jsClientHints.getString("brands"));
        Assert.assertEquals(
                "[{\"brand\":\"brand1\",\"version\":\"1.1.1\"},"
                        + "{\"brand\":\"brand2\",\"version\":\"2.2.2\"}]",
                jsClientHints.getString("fullVersionList"));
        Assert.assertEquals("2.2.2", jsClientHints.getString("uaFullVersion"));
        Assert.assertEquals("overrideTest", jsClientHints.getString("platform"));
        Assert.assertEquals("1.2.3", jsClientHints.getString("platformVersion"));
        Assert.assertEquals("x86_123", jsClientHints.getString("architecture"));
        Assert.assertEquals("foo_model", jsClientHints.getString("model"));
        Assert.assertTrue(jsClientHints.getBoolean("mobile"));
        Assert.assertEquals("128", jsClientHints.getString("bitness"));
        Assert.assertTrue(jsClientHints.getBoolean("wow64"));
        Assert.assertEquals("[\"Automotive\",\"Tablet\"]", jsClientHints.getString("formFactors"));
    }

    private Map<String, Object> makeFakeMetadata() {
        HashMap<String, Object> settings = new HashMap<>();
        settings.put(
                AwUserAgentMetadata.MetadataKeys.BRAND_VERSION_LIST,
                new String[][] {{"brand1", "1", "1.1.1"}, {"brand2", "2", "2.2.2"}});
        settings.put(AwUserAgentMetadata.MetadataKeys.FULL_VERSION, "2.2.2");
        settings.put(AwUserAgentMetadata.MetadataKeys.PLATFORM, "overrideTest");
        settings.put(AwUserAgentMetadata.MetadataKeys.PLATFORM_VERSION, "1.2.3");
        settings.put(AwUserAgentMetadata.MetadataKeys.ARCHITECTURE, "x86_123");
        settings.put(AwUserAgentMetadata.MetadataKeys.MODEL, "foo_model");
        settings.put(AwUserAgentMetadata.MetadataKeys.MOBILE, true);
        settings.put(AwUserAgentMetadata.MetadataKeys.BITNESS, 128);
        settings.put(AwUserAgentMetadata.MetadataKeys.WOW64, true);
        settings.put(
                AwUserAgentMetadata.MetadataKeys.FORM_FACTORS,
                new String[] {"Automotive", "Tablet"});
        return settings;
    }

    private String getDefaultUserAgent() throws Throwable {
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(new TestAwContentsClient())
                        .getAwContents();
        return mActivityTestRule.getAwSettingsOnUiThread(contents).getUserAgentString();
    }

    private ClientHintsTestResult getClientHintsWithOverrides(
            Map<String, Object> uaMetadataOverrides, String overrideUserAgent) throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);

        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);

        if (uaMetadataOverrides != null) {
            settings.setUserAgentMetadataFromMap(uaMetadataOverrides);
        }

        if (overrideUserAgent != null) {
            settings.setUserAgentString(overrideUserAgent);
        }

        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        ClientHintsTestResult clientHintsResult =
                makeRequestAndGetClientHints(server, contents, contentsClient);

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();

        return clientHintsResult;
    }

    private ClientHintsTestResult makeRequestAndGetClientHints(
            final AwEmbeddedTestServer server,
            final AwContents contents,
            final TestAwContentsClient contentsClient)
            throws Throwable {
        final SettableFuture<String> highEntropyResultFuture = SettableFuture.create();
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void setUserAgentClientHints(String ua) {
                        highEntropyResultFuture.set(ua);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                contents, injectedObject, "injectedObject");

        final String url =
                server.getURL(
                        "/client-hints-header?accept-ch="
                                + String.join(",", USER_AGENT_CLIENT_HINTS));

        // Load twice to be sure hints are returned, then parse the results.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        String textContent =
                mActivityTestRule
                        .getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        // Get client hints from HTTP request header.
        HashMap<String, String> clientHintsMap = getClientHints(textContent);

        // Get client hints from JS API.
        JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(),
                contents,
                contentsClient.getOnEvaluateJavaScriptResultHelper(),
                "navigator.userAgentData"
                        + ".getHighEntropyValues(['architecture', 'bitness', 'brands', "
                        + "'mobile', 'model', 'platform', 'platformVersion', 'uaFullVersion', "
                        + "'fullVersionList', 'wow64', 'formFactors'])"
                        + ".then(ua => { "
                        + "    injectedObject.setUserAgentClientHints(JSON.stringify(ua)); "
                        + "})");
        JSONObject jsonObject =
                new JSONObject(AwActivityTestRule.waitForFuture(highEntropyResultFuture));

        return new ClientHintsTestResult(clientHintsMap, jsonObject);
    }

    private void verifyClientHintBehavior(
            final AwEmbeddedTestServer server,
            final AwContents contents,
            final TestAwContentsClient contentsClient,
            boolean isSecure)
            throws Throwable {
        final String localhostURL =
                server.getURL("/client-hints-header?accept-ch=sec-ch-device-memory");
        final String fooURL =
                server.getURLWithHostName(
                        "foo.test", "/client-hints-header?accept-ch=sec-ch-device-memory");

        // First load of the localhost shouldn't have the hint as it wasn't requested before.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Second load of the localhost does have the hint as it was persisted.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);

        // Clearing cookies to clear out per-origin client hint preferences.
        clearCookies();

        // Third load of the localhost shouldn't have the hint as hint prefs were cleared.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Fourth load of the localhost does have the hint as it was persisted.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);

        // Fifth load of the localhost won't have the hint as JavaScript will be off.
        contents.getSettings().setJavaScriptEnabled(false);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        contents.getSettings().setJavaScriptEnabled(true);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // First load of foo.test shouldn't have the hint as it wasn't requested before.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), fooURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Second load of foo.test might have the hint if it the site is secure.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), fooURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", isSecure);
    }

    private void loadUrlSync(
            final AwContents contents, CallbackHelper onPageFinishedHelper, final String url)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> contents.loadUrl(url));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void validateHeadersFromJSON(
            final AwContents contents,
            final TestAwContentsClient contentsClient,
            String name,
            boolean isPresent)
            throws Throwable {
        String textContent =
                mActivityTestRule
                        .getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        HashMap<String, String> clientHintsMap = getClientHints(textContent);
        String actualVaue = clientHintsMap.get(name);
        if (isPresent) {
            Assert.assertNotEquals("HEADER_NOT_FOUND", actualVaue);
        } else {
            Assert.assertEquals("HEADER_NOT_FOUND", actualVaue);
        }
    }

    private void clearCookies() throws Throwable {
        CookieUtils.clearCookies(
                InstrumentationRegistry.getInstrumentation(), new AwCookieManager());
    }

    private void verifyUserAgentOverrideClientHints(
            final TestAwContentsClient contentsClient,
            final AwContents contents,
            String customUserAgent,
            boolean expectHighEntropyClientHints)
            throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        if (customUserAgent != null) {
            AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
            settings.setUserAgentString(customUserAgent);
        }

        final AwEmbeddedTestServer server =
                AwEmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());

        final String url =
                server.getURL(
                        "/client-hints-header?accept-ch="
                                + String.join(",", USER_AGENT_CLIENT_HINTS));

        // Load twice to be sure hints are returned, then parse the results.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        String textContent =
                mActivityTestRule
                        .getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        // JSONObject can't support parsing client hint values (like sec-ch-ua) have quote("). We
        // writes a custom parser function to approximately get the user-agent client hints in the
        // content text.
        HashMap<String, String> clientHintsMap = getClientHints(textContent);

        if (expectHighEntropyClientHints) {
            // All user-agent client hints should be in the request header.
            for (String hint : USER_AGENT_CLIENT_HINTS) {
                Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get(hint));
            }
            Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-mobile"));
            Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));
        } else {
            // Low-entropy client hints should be available.
            Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua"));
            Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get("sec-ch-ua-mobile"));
            Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));

            // High-entropy user-agent client hints should be empty.
            Assert.assertEquals("\"\"", clientHintsMap.get("sec-ch-ua-platform-version"));
            Assert.assertEquals("\"\"", clientHintsMap.get("sec-ch-ua-full-version"));
            Assert.assertEquals("", clientHintsMap.get("sec-ch-ua-full-version-list"));
        }

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    private void verifyClientHintsJavaScript(boolean useCustomUserAgent) throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        AwContents contents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentClient)
                        .getAwContents();

        if (useCustomUserAgent) {
            AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
            settings.setUserAgentString("testCustomUserAgent");
        }

        AwActivityTestRule.enableJavaScriptOnUiThread(contents);

        final SettableFuture<String> highEntropyResultFuture = SettableFuture.create();
        Object injectedObject =
                new Object() {
                    @JavascriptInterface
                    public void setUserAgentClientHints(String ua) {
                        highEntropyResultFuture.set(ua);
                    }
                };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                contents, injectedObject, "injectedObject");

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        try {
            String targetUrl = testServer.getURL("/android_webview/test/data/client_hints.html");
            loadUrlSync(contents, contentClient.getOnPageFinishedHelper(), targetUrl);
            AwActivityTestRule.pollInstrumentationThread(
                    () -> !"running".equals(mActivityTestRule.getTitleOnUiThread(contents)));
            String actualTitle = mActivityTestRule.getTitleOnUiThread(contents);
            String[] uaItems = actualTitle.split("\\|");
            // 3 navigator.userAgentData priorities.
            int expect_total_hints = 3;
            Assert.assertEquals(expect_total_hints, uaItems.length);

            // System default low-entropy user-agent client hints will always be available in
            // Javascript API even if users change the user-agent to a totally different value.

            // Verify navigator.userAgentData.platform.
            Assert.assertEquals("Android", uaItems[0]);
            // Verify navigator.userAgentData.mobile.
            Assert.assertFalse(uaItems[1].isEmpty());
            // Verify navigator.userAgentData.brands.
            Assert.assertFalse(uaItems[2].isEmpty());
            Assert.assertTrue(uaItems[2].indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);

            JSUtils.executeJavaScriptAndWaitForResult(
                    InstrumentationRegistry.getInstrumentation(),
                    contents,
                    contentClient.getOnEvaluateJavaScriptResultHelper(),
                    "navigator.userAgentData"
                            + ".getHighEntropyValues(['architecture', 'bitness', 'brands', "
                            + "'mobile', 'model', 'platform', 'platformVersion', 'uaFullVersion', "
                            + "'fullVersionList', 'wow64', 'formFactors'])"
                            + ".then(ua => { "
                            + "    injectedObject.setUserAgentClientHints(JSON.stringify(ua)); "
                            + "})");
            JSONObject jsonObject =
                    new JSONObject(AwActivityTestRule.waitForFuture(highEntropyResultFuture));

            // Verify getHighEntropyValues API.
            Assert.assertEquals(USER_AGENT_CLIENT_HINTS.length, jsonObject.length());

            if (useCustomUserAgent) {
                // low-entropy client hints should be available.
                // brands should not be empty.
                String brands = jsonObject.getString("brands");
                Assert.assertFalse(brands.isEmpty());
                Assert.assertTrue(brands.indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
                // mobile should not be empty.
                Assert.assertFalse(jsonObject.getString("mobile").isEmpty());
                // platform should return Android.
                Assert.assertEquals("Android", jsonObject.getString("platform"));

                // architecture is empty string.
                Assert.assertTrue(jsonObject.getString("architecture").isEmpty());
                // bitness is empty string.
                Assert.assertTrue(jsonObject.getString("bitness").isEmpty());
                // model is empty string.
                Assert.assertTrue(jsonObject.getString("model").isEmpty());
                // platformVersion is empty string.
                Assert.assertTrue(jsonObject.getString("platformVersion").isEmpty());
                // uaFullVersion is empty string.
                Assert.assertTrue(jsonObject.getString("uaFullVersion").isEmpty());
                // fullVersionList is empty list.
                Assert.assertEquals("[]", jsonObject.getString("fullVersionList"));
                // wow64 returns default value false.
                Assert.assertFalse(jsonObject.getBoolean("wow64"));
            } else {
                // architecture is empty string on Android.
                Assert.assertTrue(jsonObject.getString("architecture").isEmpty());
                // bitness is empty string on Android.
                Assert.assertTrue(jsonObject.getString("bitness").isEmpty());
                // brands should not be empty.
                String brands = jsonObject.getString("brands");
                Assert.assertFalse(brands.isEmpty());
                Assert.assertTrue(brands.indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
                // mobile should not be empty.
                Assert.assertFalse(jsonObject.getString("mobile").isEmpty());
                // model should not be empty on Android.
                Assert.assertFalse(jsonObject.getString("model").isEmpty());
                // platform should return Android.
                Assert.assertEquals("Android", jsonObject.getString("platform"));
                // platformVersion should not be empty.
                Assert.assertFalse(jsonObject.getString("platformVersion").isEmpty());
                // uaFullVersion should not be empty.
                Assert.assertFalse(jsonObject.getString("uaFullVersion").isEmpty());
                // fullVersionList should not be empty.
                String fullVersionList = jsonObject.getString("fullVersionList");
                Assert.assertFalse(fullVersionList.isEmpty());
                Assert.assertTrue(fullVersionList.indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
                // wow64 returns false on Android.
                Assert.assertFalse(jsonObject.getBoolean("wow64"));
            }

        } finally {
            clearCookies();
            testServer.stopAndDestroyServer();
        }
    }

    /**
     * WARNING: JSONObject can't support parsing client hint values (like sec-ch-ua) have quote(").
     * Here is the a custom parser function to approximately get the user-agent client hints in the
     * content text.
     */
    private HashMap<String, String> getClientHints(String textContent) throws Throwable {
        HashMap<String, String> result = new HashMap<>();
        if (textContent == null || textContent.length() < 2) {
            return result;
        }

        String text = textContent.substring(1, textContent.length() - 1);

        // Instead of using comma as separator, we use `,"` to parser the input to get the client
        // hints name and value pair. Some special case: "Sec-CH-UA": "Not/A)Brand";v="99", "Google
        // Chrome";v="115","Sec-CH-UA-Platform": "macOS".
        String[] hintPairs = text.split(",\"");
        int userAgentClientHintsCount = 0;
        for (String hintPair : hintPairs) {
            // Make sure we only split into two parts at the first occurrence for `:` in order to
            // handle correctly for cases when the brand value can contains special char `:`.
            String[] hints = hintPair.split(":", 2);
            if (hints.length < 2) {
                continue;
            }

            // Since we use `,"` as the separator, the client hints name could start without
            // quote(").
            String clientHintName =
                    hints[0].startsWith("\"")
                            ? hints[0].substring(1, hints[0].length() - 1)
                            : hints[0].substring(0, hints[0].length() - 1);
            String clientHintValue = hints[1].substring(1, hints[1].length() - 1);
            if (clientHintName.startsWith("sec-ch-ua")) {
                userAgentClientHintsCount++;
            }
            result.put(clientHintName, clientHintValue);
        }
        // If you're here because this line broke, please update USER_AGENT_CLIENT_HINTS to include
        // all the enabled user-agent client hints.
        Assert.assertEquals(userAgentClientHintsCount, USER_AGENT_CLIENT_HINTS.length);
        return result;
    }
}
