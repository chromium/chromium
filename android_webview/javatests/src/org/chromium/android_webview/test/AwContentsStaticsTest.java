// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.AndroidNetworkLibraryTestUtil;
import org.chromium.net.test.EmbeddedTestServer;

/** AwContentsStatics tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsStaticsTest extends AwParameterizedTest {
    private AwContents mAwContents;
    private AwTestContainerView mTestContainer;
    private TestAwContentsClient mContentsClient;

    @Rule public AwActivityTestRule mActivityTestRule;

    public AwContentsStaticsTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    /** This is necessary so we can set the cleartext setting before browser startup. */
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    private static class ClearClientCertCallbackHelper extends CallbackHelper implements Runnable {
        @Override
        public void run() {
            notifyCalled();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testClearClientCertPreferences() throws Throwable {
        mActivityTestRule.startBrowserProcess();
        final ClearClientCertCallbackHelper callbackHelper = new ClearClientCertCallbackHelper();
        int currentCallCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            // Make sure calling clearClientCertPreferences with null callback does
                            // not cause a crash.
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

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        String url = testServer.getURL("/android_webview/test/data/hello_world.html");
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(
                "onReceivedError should be called.", errorCount + 1, errorHelper.getCallCount());
        Assert.assertEquals(
                "Incorrect network error code.",
                WebviewErrorCode.ERROR_UNKNOWN,
                errorHelper.getError().errorCode);
        Assert.assertEquals(
                "onReceivedError was called for the wrong URL.", url, errorHelper.getRequest().url);
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

        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        String url = testServer.getURL("/android_webview/test/data/hello_world.html");
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        Assert.assertEquals(
                "onReceivedError should not be called.", errorCount, errorHelper.getCallCount());
    }
}
