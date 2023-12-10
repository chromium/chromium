// Copyright 2014 The Chromium Authors
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
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

/** Test MediaAccessPermissionRequest. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class MediaAccessPermissionRequestTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class OnPermissionRequestHelper extends CallbackHelper {
        private boolean mCanceled;

        public void notifyCanceled() {
            mCanceled = true;
            notifyCalled();
        }

        public boolean canceled() {
            return mCanceled;
        }
    }

    private static final String DATA =
            "<html> <script> "
                    + "var constraints = {audio: true, video: true};"
                    + "var video = document.querySelector('video');"
                    + "function successCallback(stream) {"
                    + "  window.document.title = 'grant';"
                    + "  video.srcObject = stream;"
                    + "}"
                    + "function errorCallback(error){"
                    + "  window.document.title = 'deny';"
                    + "  console.log('navigator.getUserMedia error: ', error);"
                    + "}"
                    + "navigator.webkitGetUserMedia(constraints, successCallback, errorCallback)"
                    + "</script><body>"
                    + "<video autoplay></video>"
                    + "</body></html>";

    private TestWebServer mTestWebServer;
    private String mWebRTCPage;

    public MediaAccessPermissionRequestTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mTestWebServer = TestWebServer.start();
        mWebRTCPage =
                mTestWebServer.setResponse(
                        "/WebRTC", DATA, CommonResources.getTextHtmlHeaders(true));
    }

    @After
    public void tearDown() {
        mTestWebServer.shutdown();
        mTestWebServer = null;
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testGrantAccess() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        awPermissionRequest.grant();
                        helper.notifyCalled();
                    }
                };
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        mActivityTestRule.loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        pollTitleAs("grant", awContents);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAccess() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        awPermissionRequest.deny();
                        helper.notifyCalled();
                    }
                };
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        mActivityTestRule.loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        pollTitleAs("deny", awContents);
    }

    private void pollTitleAs(final String title, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> title.equals(mActivityTestRule.getTitleOnUiThread(awContents)));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testDenyAccessByDefault() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        // Intentionally do nothing with awPermissionRequest.
                        helper.notifyCalled();
                    }
                };
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        mActivityTestRule.loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);

        // Cause AwPermissionRequest to be garbage collected, which should deny
        // the request.
        Runtime.getRuntime().gc();

        // Poll with gc in each iteration to reduce flake.
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    Runtime.getRuntime().gc();
                    return "deny".equals(mActivityTestRule.getTitleOnUiThread(awContents));
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCancelPermission() throws Throwable {
        final OnPermissionRequestHelper helper = new OnPermissionRequestHelper();
        TestAwContentsClient contentsClient =
                new TestAwContentsClient() {
                    private AwPermissionRequest mRequest;

                    @Override
                    public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                        Assert.assertNull(mRequest);
                        mRequest = awPermissionRequest;
                        // Don't respond and wait for the request canceled.
                        helper.notifyCalled();
                    }

                    @Override
                    public void onPermissionRequestCanceled(
                            AwPermissionRequest awPermissionRequest) {
                        Assert.assertNotNull(mRequest);
                        if (mRequest == awPermissionRequest) helper.notifyCanceled();
                        mRequest = null;
                    }
                };
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        int callCount = helper.getCallCount();
        mActivityTestRule.loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        callCount = helper.getCallCount();
        // Load the same page again, the previous request should be canceled.
        mActivityTestRule.loadUrlAsync(awContents, mWebRTCPage, null);
        helper.waitForCallback(callCount);
        Assert.assertTrue(helper.canceled());
    }
}
