// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.TestWebMessageListener.Data;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test the navigator.permissions.query web API in WebView.
 *
 * <p>Canonical list of permission enum constants is located at
 * https://crsrc.org/c/third_party/blink/renderer/modules/permissions/permission_descriptor.idl.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Features.EnableFeatures({ContentFeatures.WEB_PERMISSIONS_API, AwFeatures.WEBVIEW_AUTO_SAA})
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
            }
            else if (e instanceof TypeError && e.message.includes("isn't available on Android")) {
                resultListener.postMessage("not_available");
            }
            else {
                resultListener.postMessage("" + e);
            }
          });
          </script>
      """;

    /**
     * A page that queries geolocation, asks for a position, and then queries the permission state
     * again.
     */
    private static final String GEOLOCATION_PAGE_HTML =
            """
        <!DOCTYPE html>
        <html>
          <head>
            <title>Geolocation</title>
            <script>
              function gotPos(position) {
                resultListener.postMessage("position");
                navigator.permissions.query({"name": "geolocation"}).then((result) => {
                    resultListener.postMessage(result.state);
                }).catch(e => resultListener.postMessage("" + e));
              }
              function errorCallback(error){
                resultListener.postMessage("" + error);
              }
              function initiate_getCurrentPosition() {
                navigator.geolocation.getCurrentPosition(gotPos, errorCallback, { });
              }
              navigator.permissions.query({"name": "geolocation"}).then((result) => {
                resultListener.postMessage(result.state);
                initiate_getCurrentPosition();
              }).catch(e => resultListener.postMessage("" + e));
            </script>
          </head>
          <body>
          </body>
        </html>""";

    @Rule public AwActivityTestRule mActivityTestRule;

    private AwContents mAwContents;
    private TestWebMessageListener mWebMessageListener;
    private TestWebServer mServer;
    private GeolocationAwContentsClient mContentsClient;

    public AwPermissionQueryApiTest(AwSettingsMutation params) {
        this.mActivityTestRule = new AwActivityTestRule(params.getMutation());
    }

    private interface OnGeolocationCallback {

        void onGeolocationPermissionsShowPrompt(
                String origin, AwGeolocationPermissions.Callback callback);
    }

    private static class GeolocationAwContentsClient extends TestAwContentsClient {

        private OnGeolocationCallback mGeolocationCallback;

        @Override
        public void onGeolocationPermissionsShowPrompt(
                String origin, AwGeolocationPermissions.Callback callback) {
            if (mGeolocationCallback != null) {
                mGeolocationCallback.onGeolocationPermissionsShowPrompt(origin, callback);
            } else {
                super.onGeolocationPermissionsShowPrompt(origin, callback);
            }
        }

        public void setGeolocationCallback(OnGeolocationCallback geolocationCallback) {
            this.mGeolocationCallback = geolocationCallback;
        }
    }

    @Before
    public void setUp() throws Exception {

        mContentsClient = new GeolocationAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // The test mutations disable geolocation, but the tests in this class
        // all assume that geolocation is enabled, and there is an explicit test to
        // assert behavior when geolocation is disabled. Since the mutations
        // framework injects a mutation for each class, this change should not leak
        // beyond this test class.
        setGeolocationEnabledOnUiThread(true);

        mWebMessageListener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mAwContents, "resultListener", new String[] {"*"}, mWebMessageListener);

        mServer = TestWebServer.start();
    }

    private void setGeolocationEnabledOnUiThread(boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setGeolocationEnabled(enabled));
    }

    @After
    public void tearDown() throws Exception {
        mServer.close();
        // Clear all stored geolocation permissions after each test.
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.getGeolocationPermissions().clearAll());
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
        runTestCase("clipboard-write", "granted");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsPrompt() throws Exception {
        // These permissions require a user prompt.
        runTestCase("camera", "prompt");
        runTestCase("geolocation", "prompt");
        runTestCase("microphone", "prompt");
        runTestCase("midi-sysex", "prompt", "{\"name\": \"midi\", \"sysex\": true}");
        runTestCase("storage-access", "prompt");
        runTestCase(
                "top-level-storage-access",
                "prompt",
                "{\"name\": \"top-level-storage-access\", \"requestedOrigin\":"
                        + " \"https://example.com\"}");
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
        runTestCase("window-management", "denied");
        runTestCase("background-fetch", "denied");
        runTestCase("screen-wake-lock", "denied");
        runTestCase("nfc", "denied");
        runTestCase("display-capture", "denied");
        runTestCase("idle-detection", "denied");
        runTestCase("periodic-background-sync", "denied");
        runTestCase("push", "denied", "{\"name\": \"push\", \"userVisibleOnly\": true}");
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
        runTestCase("system-wake-lock", "not_enabled");
        runTestCase("local-fonts", "not_enabled");
        runTestCase("captured-surface-control", "not_enabled");
        runTestCase("speaker-selection", "not_enabled");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testPermissionsNotAvailable() throws Exception {
        // These permissions aren't available on Android.
        runTestCase("pointer-lock", "not_available");
        runTestCase("keyboard-lock", "not_available");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGeolocationDisabled() throws Exception {
        setGeolocationEnabledOnUiThread(false);

        runTestCase("geolocation", "denied");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGeolocationGranted() throws Exception {
        final String serverOrigin = mServer.getBaseUrl();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getGeolocationPermissions().allow(serverOrigin));

        runTestCase("geolocation", "granted");
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGeolocationChangeToGrantedIfRetained() throws Exception {
        MockLocationProvider provider = new MockLocationProvider();
        LocationProviderOverrider.setLocationProviderImpl(provider);
        ResettersForTesting.register(provider::stopUpdates);

        AtomicInteger callbackCounter = new AtomicInteger();
        // Set up an embedding app callback that allows the origin and retains the setting.
        final String serverOrigin = mServer.getResponseUrl("");
        mContentsClient.setGeolocationCallback(
                (origin, callback) -> {
                    callbackCounter.incrementAndGet();
                    boolean allowed = origin.startsWith(serverOrigin);
                    callback.invoke(origin, allowed, /* retain= */ true);
                });

        String pageUrl =
                mServer.setResponse(
                        "/geolocation",
                        GEOLOCATION_PAGE_HTML,
                        CommonResources.getTextHtmlHeaders(true));
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);

        Assert.assertEquals(
                "Permission should not be granted initially",
                "prompt",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "The web page did not get the current position",
                "position",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "Permission should be granted once the app has persisted the result",
                "granted",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "The app callback should only be invoked once", 1, callbackCounter.get());

        // Try to load a new page on the same origin and check that the permission is still granted.
        runTestCase("geolocation", "granted");
        Assert.assertEquals(
                "The app callback should only be invoked once", 1, callbackCounter.get());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGeolocationStaysOnPromptIfNotRetained() throws Exception {
        MockLocationProvider provider = new MockLocationProvider();
        LocationProviderOverrider.setLocationProviderImpl(provider);
        ResettersForTesting.register(provider::stopUpdates);

        AtomicInteger callbackCounter = new AtomicInteger();
        // Set up an embedding app callback that allows the origin and retains the setting.
        final String serverOrigin = mServer.getResponseUrl("");
        mContentsClient.setGeolocationCallback(
                (origin, callback) -> {
                    callbackCounter.incrementAndGet();
                    boolean allowed = origin.startsWith(serverOrigin);
                    callback.invoke(origin, allowed, /* retain= */ false);
                });

        String pageUrl =
                mServer.setResponse(
                        "/geolocation",
                        GEOLOCATION_PAGE_HTML,
                        CommonResources.getTextHtmlHeaders(true));
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);

        Assert.assertEquals(
                "Permission should not be granted initially",
                "prompt",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "The web page did not get the current position",
                "position",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "Permission should not be granted when page didn't request retain",
                "prompt",
                mWebMessageListener.waitForOnPostMessage().getAsString());
        Assert.assertEquals(
                "The app callback should only be invoked once", 1, callbackCounter.get());
    }

    private void runTestCase(String permission, String expected) throws Exception {
        runTestCase(permission, expected, String.format("{\"name\": \"%s\"}", permission));
    }

    private void runTestCase(String permission, String expected, String queryObjectString)
            throws Exception {
        String html = String.format(QUERY_API_PERMISSION, queryObjectString);
        String pageUrl =
                mServer.setResponse("/permissions", html, CommonResources.getTextHtmlHeaders(true));
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        Data data = mWebMessageListener.waitForOnPostMessage();
        Assert.assertEquals(
                "Got unexpected result for " + permission, expected, data.getAsString());
    }
}
