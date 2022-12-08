// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.util.CookieUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeUnit;

/**
 * Test suite for AwClientHintsControllerDelegate.
 * TODO(crbug.com/921655): Test Critical-CH header.
 * TODO(crbug.com/921655): Test all existing client hints.
 */
@DoNotBatch(reason = "These tests conflict with each other.")
@RunWith(AwJUnit4ClassRunner.class)
public class ClientHintsTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void testClientHintsDefault() throws Throwable {
        setupAndVerifyClientHintBehavior(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.
    Add({"disable-features=" + AwFeatures.WEBVIEW_CLIENT_HINTS_CONTROLLER_DELEGATE,
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testClientHintsDisabled() throws Throwable {
        setupAndVerifyClientHintBehavior(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=" + AwFeatures.WEBVIEW_CLIENT_HINTS_CONTROLLER_DELEGATE,
            ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
    public void
    testClientHintsEnabled() throws Throwable {
        setupAndVerifyClientHintBehavior(true);
    }

    private void setupAndVerifyClientHintBehavior(boolean isPersisted) throws Throwable {
        final TestAwContentsClient contentsClient = new TestAwContentsClient();
        final AwContents contents =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(contents);
        contents.getSettings().setJavaScriptEnabled(true);

        // First round uses insecure server.
        AwEmbeddedTestServer server = AwEmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        verifyClientHintBehavior(server, contents, contentsClient, isPersisted, false);
        clearCookies();
        server.stopAndDestroyServer();

        // Second round uses secure server.
        server = AwEmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                ServerCertificate.CERT_OK);
        verifyClientHintBehavior(server, contents, contentsClient, isPersisted, true);
        clearCookies();
        server.stopAndDestroyServer();
    }

    private void verifyClientHintBehavior(final AwEmbeddedTestServer server,
            final AwContents contents, final TestAwContentsClient contentsClient,
            boolean isPersisted, boolean isSecure) throws Throwable {
        final String localhostURL =
                server.getURL("/client-hints-header?accept-ch=sec-ch-device-memory");
        final String fooURL = server.getURLWithHostName(
                "foo.test", "/client-hints-header?accept-ch=sec-ch-device-memory");

        // First load of the localhost shouldn't have the hint as it wasn't requested before.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Second load of the localhost might have the hint if it was persisted.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", isPersisted);

        // Clearing cookies to clear out per-origin client hint preferences.
        clearCookies();

        // Third load of the localhost shouldn't have the hint as hint prefs were cleared.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Fourth load of the localhost might have the hint if it was persisted.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", isPersisted);

        // Fifth load of the localhost won't have the hint as JavaScript will be off.
        contents.getSettings().setJavaScriptEnabled(false);
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), localhostURL);
        contents.getSettings().setJavaScriptEnabled(true);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // First load of foo.test shouldn't have the hint as it wasn't requested before.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), fooURL);
        validateHeadersFromJSON(contents, contentsClient, "sec-ch-device-memory", false);

        // Second load of foo.test might have the hint if it was persisted and the site is secure.
        loadUrlSync(contents, contentsClient.getOnPageFinishedHelper(), fooURL);
        validateHeadersFromJSON(
                contents, contentsClient, "sec-ch-device-memory", isPersisted && isSecure);
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
}
