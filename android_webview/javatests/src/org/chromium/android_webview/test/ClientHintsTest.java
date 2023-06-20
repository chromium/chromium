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

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.HashMap;
import java.util.concurrent.TimeUnit;

/**
 * Test suite for AwClientHintsControllerDelegate.
 */
@DoNotBatch(reason = "These tests conflict with each other.")
@RunWith(AwJUnit4ClassRunner.class)
public class ClientHintsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String[] USER_AGENT_CLIENT_HINTS = {"sec-ch-ua", "sec-ch-ua-arch",
            "sec-ch-ua-platform", "sec-ch-ua-model", "sec-ch-ua-mobile", "sec-ch-ua-full-version",
            "sec-ch-ua-platform-version", "sec-ch-ua-bitness", "sec-ch-ua-full-version-list",
            "sec-ch-ua-wow64"};

    private static final String ANDROID_WEBVIEW_BRAND_NAME = "Android WebView";

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testClientHintsDefault() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);

        // First round uses insecure server.
        AwEmbeddedTestServer server = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        verifyClientHintBehavior(server, contents, contentsClient, false);
        clearCookies();
        server.stopAndDestroyServer();

        // Second round uses secure server.
        server = AwEmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                ServerCertificate.CERT_OK);
        verifyClientHintBehavior(server, contents, contentsClient, true);
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testAllClientHints() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        final AwEmbeddedTestServer server = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Please keep these here (and below) in the same order as web_client_hints_types.mojom.
        final String url = server.getURL(
                "/client-hints-header?accept-ch=device-memory,dpr,width,viewport-width,"
                + "rtt,downlink,ect,sec-ch-lang,sec-ch-ua,sec-ch-ua-arch,sec-ch-ua-platform,"
                + "sec-ch-ua-model,sec-ch-ua-mobile,sec-ch-ua-full-version,"
                + "sec-ch-ua-platform-version,sec-ch-prefers-color-scheme,"
                + "sec-ch-ua-bitness,sec-ch-viewport-height,"
                + "sec-ch-device-memory,sec-ch-dpr,sec-ch-width,sec-ch-viewport-width,"
                + "sec-ch-ua-full-version-list,sec-ch-ua-wow64,save-data,"
                + "sec-ch-prefers-reduced-motion");

        // Load twice to be sure hints are returned, then parse the results.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        JSONObject jsonObject = new JSONObject(textContent);
        // If you're here because this line broke, please update this test to verify whichever
        // client hints were added or removed and don't just modify the number below.
        Assert.assertEquals(25, jsonObject.length());

        // All client hints must be verified for default behavior.
        Assert.assertTrue(jsonObject.getInt("device-memory") > 0);
        Assert.assertTrue(jsonObject.getDouble("dpr") > 0);
        // This is only set for subresources.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("width"));
        Assert.assertTrue(jsonObject.getInt("viewport-width") > 0);
        Assert.assertEquals(0, jsonObject.getInt("rtt"));
        Assert.assertEquals(0, jsonObject.getInt("downlink"));
        // This is the holdback value (the default in some cases).
        Assert.assertEquals("4g", jsonObject.getString("ect"));
        // This client hint was removed.
        Assert.assertFalse(jsonObject.has("sec-ch-lang"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-arch"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-platform"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-model"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-mobile"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-full-version"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-platform-version"));
        Assert.assertEquals("light", jsonObject.getString("sec-ch-prefers-color-scheme"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-bitness"));
        Assert.assertTrue(jsonObject.getInt("sec-ch-viewport-height") > 0);
        Assert.assertTrue(jsonObject.getInt("sec-ch-device-memory") > 0);
        Assert.assertTrue(jsonObject.getDouble("sec-ch-dpr") > 0);
        // This is only set for subresources.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-width"));
        Assert.assertTrue(jsonObject.getInt("sec-ch-viewport-width") > 0);
        // User agent client hints are inactive on android webview.
        Assert.assertEquals(
                "HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-full-version-list"));
        // User agent client hints are inactive on android webview.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-ua-wow64"));
        // This client hint isn't sent when data-saver is off.
        Assert.assertEquals("HEADER_NOT_FOUND", jsonObject.getString("save-data"));
        Assert.assertNotEquals("HEADER_NOT_FOUND", jsonObject.getString("sec-ch-prefers-reduced-motion"));

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint",
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testEnableUserAgentClientHintsNoCustom() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/null,
                /*expectUserAgentClientExist=*/true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint",
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testEnableUserAgentClientHintsCustomOverride() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/"CustomUserAgentOverride",
                /*expectUserAgentClientExist=*/false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint",
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testEnableUserAgentClientHintsModifyDefaultUserAgent() throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
        String defaultUserAgent = settings.getUserAgentString();

        // Override user-agent with appending suffix.
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/defaultUserAgent + "CustomUserAgentSuffix",
                /*expectUserAgentClientExist=*/true);

        // Override user-agent with adding prefix.
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/"CustomUserAgentPrefix" + defaultUserAgent,
                /*expectUserAgentClientExist=*/true);

        // Override user-agent with adding both prefix and suffix.
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/"CustomUserAgentPrefix" + defaultUserAgent
                        + "CustomUserAgentSuffix",
                /*expectUserAgentClientExist=*/true);

        // Override user-agent with empty string, it's assumed to use system default.
        verifyUserAgentOverrideClientHints(/*contentsClient=*/contentsClient,
                /*contents=*/contents,
                /*customUserAgent=*/"",
                /*expectUserAgentClientExist=*/true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint",
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testEnableUserAgentClientHintsJavaScript() throws Throwable {
        verifyClientHintsJavaScript(/*useCustomUserAgent=*/false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Preferences"})
    @CommandLineFlags.Add({"enable-features=UserAgentClientHint",
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testEnableUserAgentClientHintsOverrideJavaScript() throws Throwable {
        verifyClientHintsJavaScript(/*useCustomUserAgent=*/true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testCriticalClientHints() throws Throwable {
        // Initial test setup.
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        final AwEmbeddedTestServer server = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        // First we verify that sec-ch-device-memory (critical) is returned on the first load.
        String url = server.getURL("/critical-client-hints-header?accept-ch=sec-ch-device-memory&"
                + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", false);

        // Second we verify that device-memory (not critical) won't cause a reload.
        url = server.getURL(
                "/critical-client-hints-header?accept-ch=sec-ch-device-memory,device-memory&"
                + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", false);

        // Third we verify that device-memory is returned on the final load even with no request.
        url = server.getURL("/critical-client-hints-header?accept-ch=sec-ch-device-memory&"
                + "critical-ch=sec-ch-device-memory");
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", true);
        validateHeadersFromJSON(contents, contentsClient, "device-memory", true);

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    private void verifyClientHintBehavior(final AwEmbeddedTestServer server,
            final AwContents contents, final TestAwContentsClient contentsClient, boolean isSecure)
            throws Throwable {
        final String localhostURL =
                server.getURL("/client-hints-header?accept-ch=sec-ch-device-memory");
        final String fooURL = server.getURLWithHostName(
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

    private void loadUrlSync(final AwContents contents, CallbackHelper onPageFinishedHelper,
            final String url) throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> contents.loadUrl(url));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void validateHeadersFromJSON(final AwContents contents,
            final TestAwContentsClient contentsClient, String name, boolean isPresent)
            throws Exception {
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        JSONObject jsonObject = new JSONObject(textContent);
        String actualVaue = jsonObject.getString(name);
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

    private void verifyUserAgentOverrideClientHints(final TestAwContentsClient contentsClient,
            final AwContents contents, String customUserAgent, boolean expectUserAgentClientExist)
            throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);
        if (customUserAgent != null) {
            AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
            settings.setUserAgentString(customUserAgent);
        }

        final AwEmbeddedTestServer server = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        final String url = server.getURL(
                "/client-hints-header?accept-ch=" + String.join(",", USER_AGENT_CLIENT_HINTS));

        // Load twice to be sure hints are returned, then parse the results.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), url);
        String textContent =
                mActivityTestRule.getJavaScriptResultBodyTextContent(contents, contentsClient)
                        .replaceAll("\\\\\"", "\"");
        // JSONObject can't support parsing client hint values (like sec-ch-ua) have quote("). We
        // writes a custom parser function to approximately get the user-agent client hints in the
        // content text.
        HashMap<String, String> clientHintsMap = getClientHints(textContent);

        if (expectUserAgentClientExist) {
            // All user-agent client hints should be in the request header.
            for (String hint : USER_AGENT_CLIENT_HINTS) {
                Assert.assertNotEquals("HEADER_NOT_FOUND", clientHintsMap.get(hint));
            }
            Assert.assertFalse(clientHintsMap.get("sec-ch-ua-mobile").isEmpty());
            Assert.assertEquals("\"Android\"", clientHintsMap.get("sec-ch-ua-platform"));
        } else {
            // No user-agent client hints on the request headers.
            for (String hint : USER_AGENT_CLIENT_HINTS) {
                Assert.assertEquals("HEADER_NOT_FOUND", clientHintsMap.get(hint));
            }
        }

        // Cleanup after test.
        clearCookies();
        server.stopAndDestroyServer();
    }

    private void verifyClientHintsJavaScript(boolean useCustomUserAgent) throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        AwContents contents = mActivityTestRule.createAwTestContainerViewOnMainSync(contentClient)
                                      .getAwContents();

        if (useCustomUserAgent) {
            AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(contents);
            settings.setUserAgentString("testCustomUserAgent");
        }

        AwActivityTestRule.enableJavaScriptOnUiThread(contents);

        final SettableFuture<String> highEntropyResultFuture = SettableFuture.create();
        Object injectedObject = new Object() {
            @JavascriptInterface
            public void setUserAgentClientHints(String ua) {
                highEntropyResultFuture.set(ua);
            }
        };
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                contents, injectedObject, "injectedObject");

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
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

            if (useCustomUserAgent) {
                // As we override the user-agent to a totally different value, system default
                // user-agent client hints won't be available in Javascript API.

                // Verify navigator.userAgentData.platform.
                Assert.assertTrue(uaItems[0].isEmpty());
                // Verify navigator.userAgentData.mobile.
                Assert.assertEquals("false", uaItems[1]);
                // Verify navigator.userAgentData.brands.
                Assert.assertEquals("[]", uaItems[2]);
            } else {
                // Verify navigator.userAgentData.platform.
                Assert.assertEquals("Android", uaItems[0]);
                // Verify navigator.userAgentData.mobile.
                Assert.assertFalse(uaItems[1].isEmpty());
                // Verify navigator.userAgentData.brands.
                Assert.assertFalse(uaItems[2].isEmpty());
                Assert.assertTrue(uaItems[2].indexOf(ANDROID_WEBVIEW_BRAND_NAME) != -1);
            }

            JSUtils.executeJavaScriptAndWaitForResult(InstrumentationRegistry.getInstrumentation(),
                    contents, contentClient.getOnEvaluateJavaScriptResultHelper(),
                    "navigator.userAgentData"
                            + ".getHighEntropyValues(['architecture', 'bitness', 'brands', "
                            + "'mobile', 'model', 'platform', 'platformVersion', 'uaFullVersion', "
                            + "'fullVersionList', 'wow64'])"
                            + ".then(ua => { "
                            + "    injectedObject.setUserAgentClientHints(JSON.stringify(ua)); "
                            + "})");
            JSONObject jsonObject =
                    new JSONObject(AwActivityTestRule.waitForFuture(highEntropyResultFuture));

            // Verify getHighEntropyValues API.
            Assert.assertEquals(USER_AGENT_CLIENT_HINTS.length, jsonObject.length());

            if (useCustomUserAgent) {
                // architecture is empty string.
                Assert.assertTrue(jsonObject.getString("architecture").isEmpty());
                // bitness is empty string.
                Assert.assertTrue(jsonObject.getString("bitness").isEmpty());
                // brands is empty list.
                Assert.assertEquals("[]", jsonObject.getString("brands"));
                // mobile returns default value false.
                Assert.assertFalse(jsonObject.getBoolean("mobile"));
                // model is empty string.
                Assert.assertTrue(jsonObject.getString("model").isEmpty());
                // platform is empty string.
                Assert.assertTrue(jsonObject.getString("platform").isEmpty());
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

    private HashMap<String, String> getClientHints(String textContent) throws Throwable {
        HashMap<String, String> result = new HashMap<>();
        if (textContent == null || textContent.length() < 2) {
            return result;
        }

        String text = textContent.substring(1, textContent.length() - 1);
        String[] hintPairs = text.split(",");
        int userAgentClientHintsCount = 0;
        for (String hintPair : hintPairs) {
            String[] hints = hintPair.split(":");
            if (hints.length < 2) {
                continue;
            }
            String clientHintName = hints[0].substring(1, hints[0].length() - 1);
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
