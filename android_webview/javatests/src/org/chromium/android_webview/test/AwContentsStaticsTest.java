// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.ErrorCodeConversionHelper;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedError2Helper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.AndroidNetworkLibraryTestUtil;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * AwContentsStatics tests.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsStaticsTest {
    private AwContents mAwContents;
    private AwTestContainerView mTestContainer;
    private TestAwContentsClient mContentsClient;

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule() {
        /**
         * This is necessary so we can set the cleartext setting before browser startup.
         */
        @Override
        public boolean needsBrowserProcessStarted() {
            return false;
        }
    };

    private static class ClearClientCertCallbackHelper extends CallbackHelper
            implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testClearClientCertPreferences() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final ClearClientCertCallbackHelper callbackHelper = new ClearClientCertCallbackHelper();
        int currentCallCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            // Make sure calling clearClientCertPreferences with null callback does not
            // cause a crash.
            AwContentsStatics.clearClientCertPreferences(null);
            AwContentsStatics.clearClientCertPreferences(callbackHelper);
        });
        callbackHelper.waitForCallback(currentCallCount);
    }

    private void createContainerView() {
        mContentsClient = new TestAwContentsClient();
        mTestContainer = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainer.getAwContents();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSetCheckClearTextPermittedTrue() throws Throwable {
        // Note: must setCheckClearTextPermitted() before starting browser process.
        AwContentsStatics.setCheckClearTextPermitted(true);
        mActivityTestRule.startBrowserProcess();

        // Set the policy to reject cleartext, otherwise there's nothing to validate.
        AndroidNetworkLibraryTestUtil.setUpSecurityPolicyForTesting(false);

        createContainerView();

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            String url = testServer.getURL("/android_webview/test/data/hello_world.html");
            OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
            int errorCount = errorHelper.getCallCount();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals("onReceivedError should be called.", errorCount + 1,
                    errorHelper.getCallCount());
            Assert.assertEquals("Incorrect network error code.",
                    ErrorCodeConversionHelper.ERROR_UNKNOWN, errorHelper.getError().errorCode);
            Assert.assertEquals("onReceivedError was called for the wrong URL.", url,
                    errorHelper.getRequest().url);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSetCheckClearTextPermittedFalse() throws Throwable {
        // Note: must setCheckClearTextPermitted() before starting browser process.
        AwContentsStatics.setCheckClearTextPermitted(false);
        mActivityTestRule.startBrowserProcess();

        // Set the policy to reject cleartext, otherwise there's nothing to validate.
        AndroidNetworkLibraryTestUtil.setUpSecurityPolicyForTesting(false);

        createContainerView();

        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            String url = testServer.getURL("/android_webview/test/data/hello_world.html");
            OnReceivedError2Helper errorHelper = mContentsClient.getOnReceivedError2Helper();
            int errorCount = errorHelper.getCallCount();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            Assert.assertEquals("onReceivedError should not be called.", errorCount,
                    errorHelper.getCallCount());
        } finally {
            testServer.stopAndDestroyServer();
        }
    }
}
