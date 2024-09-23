// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONException;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.BuildInfo;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.content_public.browser.test.util.DomAutomationController;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/** Test AwPermissionManager. */
@DoNotBatch(
        reason =
                "Storage Access tests load from manifest and have global state. Run them separately"
                        + " to avoid state flowing between tests.")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwPermissionManagerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String REQUEST_DUPLICATE =
            "<html> <script>"
                    + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
                    + "});"
                    + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
                    + "  window.document.title = 'second-granted';"
                    + "});"
                    + "</script><body>"
                    + "</body></html>";

    private static final String EMPTY_PAGE =
            "<html><script>" + "</script><body>" + "</body></html>";

    private static final String IFRAME_PARENT_PAGE = "<html><iframe></iframe><body></body></html>";

    private static final String REQUEST_STORAGE_ACCESS_PAGE =
            """
            <html>
            <body>
            <script>
            document.requestStorageAccess()
                .then(() => window.parent.postMessage('granted', '*'))
                .catch((e) => window.parent.postMessage('not granted', '*'));
            </script>
            </body>
            </html>""";

    private static final String GUM_JS =
            "navigator.mediaDevices.getUserMedia({video: true, audio: true})"
                    + ".then((_) => domAutomationController.send('success'))"
                    + ".catch((error) => domAutomationController.send('failure'));";

    private static final String ENUMERATE_DEVICES_JS =
            "navigator.mediaDevices.enumerateDevices().then("
                    + "(devices) => domAutomationController.send(devices.map("
                    + "  (d) => `${d['label']}`)));";

    private static final String ASSET_STATEMENT_TEMPLATE =
            """
                [{
                        "relation": ["delegate_permission/common.handle_all_urls"],
                        "target": {
                                "namespace": "android_app",
                                "package_name": "%s",
                                "sha256_cert_fingerprints": ["%s"]
                        }
                }]
        """;

    private static final String ASSET_STATEMENT_INCLUDE_CONTENT =
            """
        [{
            "relation": ["delegate_permission/common.handle_all_urls"],
            "target": {
              "namespace": "web",
              "site": "http://127.0.0.1"
            }
        }]
        """;

    private static final String ASSET_STATEMENT_PATH = "/.well-known/assetlinks.json";
    private static final int ASSET_STATEMENT_INCLUDE_PORT = 2024;
    private static final String ASSET_STATEMENT_INCLUDE_PATH = "/includedstatements.json";
    private static final String SAA_GRANT_TIME_HISTOGRAM =
            "Android.WebView.StorageAccessAutoGrantTime";

    private final DomAutomationController mDomAutomationController = new DomAutomationController();
    private TestWebServer mTestWebServer;
    private String mPage;
    private TestAwContentsClient mContentsClient;

    public AwPermissionManagerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mTestWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mTestWebServer.shutdown();
        mTestWebServer = null;
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testRequestMultiple() {
        mPage =
                mTestWebServer.setResponse(
                        "/permissions",
                        REQUEST_DUPLICATE,
                        CommonResources.getTextHtmlHeaders(true));

        mContentsClient =
                new TestAwContentsClient() {
                    private boolean mCalled;

                    @Override
                    public void onPermissionRequest(final AwPermissionRequest awPermissionRequest) {
                        if (mCalled) {
                            Assert.fail("Only one request was expected");
                            return;
                        }
                        mCalled = true;

                        // Emulate a delayed response to the request by running four seconds in the
                        // future.
                        Handler handler = new Handler(Looper.myLooper());
                        handler.postDelayed(awPermissionRequest::grant, 4000);
                    }
                };

        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlAsync(awContents, mPage, null);
        pollTitleAs("second-granted", awContents);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testRequestMediaPermissions() throws Exception {
        AwContents awContents = setUpEnumerateDevicesTest(null);

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), mPage);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);
        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testNavigationRevokesEnumerateDevicesLabelsPermissions() throws Exception {
        AwContents awContents = setUpEnumerateDevicesTest(null);

        TestWebServer secondServer = TestWebServer.startAdditional();
        String secondPage =
                secondServer.setResponse(
                        "/new-page", EMPTY_PAGE, CommonResources.getTextHtmlHeaders(true));

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), mPage);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        // Navigate to a page with a different origin.
        mActivityTestRule.loadUrlSync(
                awContents, mContentsClient.getOnPageFinishedHelper(), secondPage);
        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testEnumerateDevicesWithAllowFileAccessFromFileURLsFalse() throws Throwable {
        AwContents awContents = setUpEnumerateDevicesTest(null);
        awContents.getSettings().setAllowFileAccessFromFileURLs(false);
        mActivityTestRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                EMPTY_PAGE,
                "text/html",
                true,
                "file:///foo.html",
                "");
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);

        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testEnumerateDevicesWithAllowFileAccessFromFileURLsTrue() throws Throwable {
        AwContents awContents = setUpEnumerateDevicesTest(null);
        awContents.getSettings().setAllowFileAccessFromFileURLs(true);
        mActivityTestRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                EMPTY_PAGE,
                "text/html",
                true,
                "file:///foo.html",
                "");
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);

        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, false);
    }

    // Test that a successful getUserMedia grants enumerateDevices permission for
    // all file:/// URLs when AllowFileAccessFromFileURLs is enabled.
    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testPermissionIsCachedAfterFileNavigation() throws Throwable {
        AwContents awContents = setUpEnumerateDevicesTest(null);
        awContents.getSettings().setAllowFileAccessFromFileURLs(true);
        mActivityTestRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                EMPTY_PAGE,
                "text/html",
                true,
                "file:///foo.html",
                "");
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);

        // Navigate to a different file URL.
        mActivityTestRule.loadDataWithBaseUrlSync(
                awContents,
                mContentsClient.getOnPageFinishedHelper(),
                EMPTY_PAGE,
                "text/html",
                true,
                "file:///bar.html",
                "");

        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testEnumerateDevicesDoesNotShowLabelsBeforeGetUserMedia() throws Exception {
        AwContents awContents = setUpEnumerateDevicesTest(null);

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), mPage);
        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);

        assertDeviceLabels(devices, true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testRevokeEnumerateDevicesPermission() throws Exception {
        AwContents awContents =
                setUpEnumerateDevicesTest(
                        new TestAwContentsClient() {
                            private boolean mHasBeenGranted;

                            @Override
                            public void onPermissionRequest(
                                    AwPermissionRequest awPermissionRequest) {
                                if (mHasBeenGranted) {
                                    awPermissionRequest.deny();
                                    return;
                                }
                                mHasBeenGranted = true;
                                awPermissionRequest.grant();
                            }
                        });

        mActivityTestRule.loadUrlSync(awContents, mContentsClient.getOnPageFinishedHelper(), mPage);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), ENUMERATE_DEVICES_JS);
        JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(), GUM_JS);
        String devices =
                JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                        awContents.getWebContents(), ENUMERATE_DEVICES_JS);
        assertDeviceLabels(devices, true);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({AwFeatures.WEBVIEW_AUTO_SAA})
    public void testAutoGrantSAA_trusted() throws Exception {
        String result;
        var buildInfo = BuildInfo.getInstance();

        // We add an asset statement to always trust the test app for auto granting.
        mTestWebServer.setResponse(
                ASSET_STATEMENT_PATH,
                String.format(
                        ASSET_STATEMENT_TEMPLATE,
                        buildInfo.hostPackageName,
                        buildInfo.getHostSigningCertSha256()),
                null);

        try (var histogramWatcher =
                HistogramWatcher.newBuilder().expectAnyRecord(SAA_GRANT_TIME_HISTOGRAM).build()) {
            result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ true);
            Assert.assertEquals("\"granted\"", result);
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
        // Confirm this is resolved against the test server the first time
        Assert.assertEquals(1, mTestWebServer.getRequestCount(ASSET_STATEMENT_PATH));

        result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ true);
        Assert.assertEquals("\"granted\"", result);
        // Confirm that subsequent calls are from cached results
        Assert.assertEquals(1, mTestWebServer.getRequestCount(ASSET_STATEMENT_PATH));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({
        AwFeatures.WEBVIEW_AUTO_SAA,
        AwFeatures.WEBVIEW_DIGITAL_ASSET_LINKS_LOAD_INCLUDES
    })
    public void testAutoGrantSAA_trustedViaInclude() throws Exception {
        // We add an asset statement to always trust the test app for auto granting.
        var buildInfo = BuildInfo.getInstance();
        mTestWebServer.setResponse(
                ASSET_STATEMENT_PATH,
                String.format(
                        ASSET_STATEMENT_TEMPLATE,
                        buildInfo.hostPackageName,
                        buildInfo.getHostSigningCertSha256()),
                null);
        String result;

        try (var histogramWatcher =
                        HistogramWatcher.newBuilder()
                                .expectAnyRecord(SAA_GRANT_TIME_HISTOGRAM)
                                .build();
                TestWebServer includeServer =
                        TestWebServer.startAdditional(ASSET_STATEMENT_INCLUDE_PORT)) {
            // We make sure that the include path returns an include for "http://127.0.0.1
            includeServer.setResponse(
                    ASSET_STATEMENT_INCLUDE_PATH,
                    ASSET_STATEMENT_INCLUDE_CONTENT,
                    List.of(Pair.create("Content-Type", "application/json")));

            result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ false);

            Assert.assertEquals("\"granted\"", result);
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();

            // Confirm this is resolved against the test server the first time
            Assert.assertEquals(1, mTestWebServer.getRequestCount(ASSET_STATEMENT_PATH));

            result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ false);
            Assert.assertEquals("\"granted\"", result);
            // Confirm that subsequent calls are from cached results
            Assert.assertEquals(1, mTestWebServer.getRequestCount(ASSET_STATEMENT_PATH));
            // Confirm we only load the include path once
            Assert.assertEquals(1, includeServer.getRequestCount(ASSET_STATEMENT_INCLUDE_PATH));
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({AwFeatures.WEBVIEW_AUTO_SAA})
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_DIGITAL_ASSET_LINKS_LOAD_INCLUDES
    })
    public void testAutoGrantSAA_untrustedDomain() throws Exception {
        var buildInfo = BuildInfo.getInstance();

        // We add an asset statement to always trust the test app for auto granting.
        mTestWebServer.setResponse(
                ASSET_STATEMENT_PATH,
                String.format(
                        ASSET_STATEMENT_TEMPLATE,
                        buildInfo.hostPackageName,
                        buildInfo.getHostSigningCertSha256()),
                null);

        try (var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(SAA_GRANT_TIME_HISTOGRAM).build()) {
            String result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ false);
            Assert.assertEquals("\"not granted\"", result);
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @Features.EnableFeatures({AwFeatures.WEBVIEW_AUTO_SAA})
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_DIGITAL_ASSET_LINKS_LOAD_INCLUDES
    })
    public void testAutoGrantSAA_untrustedApp() throws Exception {
        // In this test's case, we make the site only trust an app we are not.
        mTestWebServer.setResponse(
                ASSET_STATEMENT_PATH,
                String.format(ASSET_STATEMENT_TEMPLATE, "some other app", "some hash"),
                null);
        try (var histogramWatcher =
                HistogramWatcher.newBuilder().expectAnyRecord(SAA_GRANT_TIME_HISTOGRAM).build()) {
            String result = requestEmbeddedStorageAccess(/* useLocalhostOrigin= */ true);
            Assert.assertEquals("\"not granted\"", result);
            histogramWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    /**
     * Run the test for embedded storage access auto grant.
     *
     * @param useLocalhostOrigin {@code true} means that the web site will be loaded using {@code
     *     http://localhost}, {@code false} means that the WebView will instead load {@code
     *     http://127.0.0.1}.
     * @return String sent to {@code domAutomationController}.
     */
    private String requestEmbeddedStorageAccess(boolean useLocalhostOrigin) throws Exception {
        var contentsClient = new TestAwContentsClient();
        final AwContents awContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(contentsClient)
                        .getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        // We need to request storage access from within an iframe, otherwise it will
        // just auto resolve to granted.
        // The iframe will load, request storage access, and post the result back.
        var storagePage = mTestWebServer.setResponse("/storage", REQUEST_STORAGE_ACCESS_PAGE, null);
        var parentPage = mTestWebServer.setResponse("/", IFRAME_PARENT_PAGE, null);

        // The test app trusts localhost. To test a flow where we don't have
        // the website in our apps asset statement, we can just use a IP address
        // that the app hasn't declared but still resolves.
        if (!useLocalhostOrigin) {
            storagePage = storagePage.replace("localhost", "127.0.0.1");
            parentPage = parentPage.replace("localhost", "127.0.0.1");
        }

        mActivityTestRule.loadUrlSync(
                awContents, contentsClient.getOnPageFinishedHelper(), parentPage);

        // We add an event listener for the result from the iframe and then initiate the page
        // load.
        return JavaScriptUtils.runJavascriptWithUserGestureAndAsyncResult(
                awContents.getWebContents(),
                String.format(
                        """
                                window.addEventListener('message', (e) => {
                                        domAutomationController.send(e.data)
                                });
                                document.querySelector('iframe').src = "%s";""",
                        storagePage));
    }

    private void pollTitleAs(final String title, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> title.equals(mActivityTestRule.getTitleOnUiThread(awContents)));
    }

    private AwContents setUpEnumerateDevicesTest(@Nullable TestAwContentsClient contentsClient)
            throws Exception {
        mPage =
                mTestWebServer.setResponse(
                        "/media", EMPTY_PAGE, CommonResources.getTextHtmlHeaders(true));

        mContentsClient =
                contentsClient != null
                        ? contentsClient
                        : new TestAwContentsClient() {
                            @Override
                            public void onPermissionRequest(
                                    final AwPermissionRequest awPermissionRequest) {
                                awPermissionRequest.grant();
                            }
                        };

        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testContainerView.getAwContents();
        mDomAutomationController.inject(awContents.getWebContents());
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);

        return awContents;
    }

    private void assertDeviceLabels(String devices, boolean shouldBeEmpty) throws JSONException {
        JSONArray devicesJson = new JSONArray(devices);
        boolean isEmpty = true;
        for (int i = 0; i < devicesJson.length(); i++) {
            if (!devicesJson.getString(i).isEmpty()) {
                isEmpty = false;
                break;
            }
        }
        if (shouldBeEmpty) {
            Assert.assertTrue(isEmpty);
        } else {
            Assert.assertFalse(isEmpty);
        }
    }
}
