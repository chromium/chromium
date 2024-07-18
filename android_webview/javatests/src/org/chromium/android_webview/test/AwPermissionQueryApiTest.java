// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;

/**
 * Test the navigator.permissions.query web API in WebView.
 *
 * <p>Canonical list of permission enum constants is located at
 * https://crsrc.org/c/third_party/blink/renderer/modules/permissions/permission_descriptor.idl.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@CommandLineFlags.Add({"enable-features=" + ContentFeatures.WEB_PERMISSIONS_API})
@Batch(Batch.PER_CLASS)
public class AwPermissionQueryApiTest extends AwParameterizedTest {

    // Script template to query a permission and report the result back using the injected listener.
    private static final String QUERY_API_PERMISSION =
            """
          <html>
          <script>
          navigator.permissions.query(%s).then((result) => {
            resultListener.postMessage(result.state);
          }).catch(e => {
            if (e instanceof TypeError && e.message.includes("is not enabled")) {
                resultListener.postMessage("not_enabled");
            } else {
                resultListener.postMessage("" + e);
            }
          });
          </script>
      """;

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;

    public AwPermissionQueryApiTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        TestAwContentsClient mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsAllowed() throws Exception {
        // These permissions are auto-granted by WebView.
        runTestCase("accelerometer", "granted");
        runTestCase("gyroscope", "granted");
        runTestCase("midi", "granted");
        runTestCase("magnetometer", "granted");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsPrompt() throws Exception {

        // These permissions require a user prompt.
        runTestCase("camera", "prompt");
        runTestCase("clipboard-write", "prompt");
        runTestCase("geolocation", "prompt");
        runTestCase("microphone", "prompt");
        runTestCase("midi-sysex", "prompt", "{\"name\": \"midi\", \"sysex\": true}");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsDenied() throws Exception {
        // These permissions are not supported by WebView and auto-denied.
        runTestCase("background-sync", "denied");
        runTestCase("clipboard-read", "denied");
        runTestCase("notifications", "denied");
        runTestCase("payment-handler", "denied");
        runTestCase("persistent-storage", "denied");
        runTestCase("screen-wake-lock", "denied");
        runTestCase("storage-access", "denied");
        runTestCase("window-management", "denied");
        runTestCase("background-fetch", "denied");
        runTestCase("screen-wake-lock", "denied");
        runTestCase("nfc", "denied");
        runTestCase("display-capture", "denied");
        runTestCase("idle-detection", "denied");
        runTestCase("periodic-background-sync", "denied");
        runTestCase("keyboard-lock", "denied");
        runTestCase("push", "denied", "{\"name\": \"push\", \"userVisibleOnly\": true}");
        runTestCase(
                "top-level-storage-access",
                "denied",
                "{\"name\": \"top-level-storage-access\", \"requestedOrigin\":"
                        + " \"https://example.com\"}");
        runTestCase("pointer-lock", "denied");
        runTestCase(
                "fullscreen",
                "denied",
                "{\"name\": \"fullscreen\", \"allowWithoutGesture\": true}");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsNotEnabled() throws Exception {
        // These permissions are blocked behind feature flags that are not enabled
        // in WebView.
        runTestCase("ambient-light-sensor", "not_enabled");
        runTestCase("accessibility-events", "not_enabled");
        runTestCase("system-wake-lock", "not_enabled");
        runTestCase("local-fonts", "not_enabled");
        runTestCase("captured-surface-control", "not_enabled");
        runTestCase("speaker-selection", "not_enabled");
    }

    private void runTestCase(String permission, String expected) throws Exception {
        runTestCase(permission, expected, String.format("{\"name\": \"%s\"}", permission));
    }

    private void runTestCase(String permission, String expected, String queryObjectString)
            throws Exception {
        try (TestWebServer server = TestWebServer.start()) {
            String html = String.format(QUERY_API_PERMISSION, queryObjectString);
            String pageUrl =
                    server.setResponse(
                            "/permissions", html, CommonResources.getTextHtmlHeaders(true));
            mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
            Data data = mWebMessageListener.waitForOnPostMessage();
            Assert.assertEquals(
                    "Got unexpected result for " + permission, expected, data.getAsString());
        }
    }
}
