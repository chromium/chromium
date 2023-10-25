// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Handler;
import android.os.Looper;

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

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.DomAutomationController;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

/** Test AwPermissionManager. */
@RunWith(AwJUnit4ClassRunner.class)
public class AwPermissionManagerTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

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

    private static final String GUM_JS =
            "navigator.mediaDevices.getUserMedia({video: true, audio: true})"
                    + ".then((_) => domAutomationController.send('success'))"
                    + ".catch((error) => domAutomationController.send('failure'));";

    private static final String ENUMERATE_DEVICES_JS =
            "navigator.mediaDevices.enumerateDevices().then("
                    + "(devices) => domAutomationController.send(devices.map("
                    + "  (d) => `${d['label']}`)));";

    private final DomAutomationController mDomAutomationController = new DomAutomationController();
    private TestWebServer mTestWebServer;
    private String mPage;
    private TestAwContentsClient mContentsClient;

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
