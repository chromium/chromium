// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.AwQuotaManagerBridgeTestUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.net.test.util.TestWebServer;

/** Tests for the AwQuotaManagerBridge. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwQuotaManagerBridgeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private String mOrigin;

    public AwQuotaManagerBridgeTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.start();
        mOrigin = mWebServer.getBaseUrl();

        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
    }

    @After
    public void tearDown() {
        deleteAllData();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    private void deleteAllData() {
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> bridge.deleteAllData());
    }

    private void deleteOrigin(final String origin) {
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> bridge.deleteOrigin(origin));
    }

    private static class LongValueCallbackHelper extends CallbackHelper {
        private long mValue;

        public void notifyCalled(long value) {
            mValue = value;
            notifyCalled();
        }

        public long getValue() {
            assert getCallCount() > 0;
            return mValue;
        }
    }

    private long getQuotaForOrigin() throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();

        int callCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                bridge.getQuotaForOrigin(
                                        "foo.com", quota -> callbackHelper.notifyCalled(quota)));
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    private long getUsageForOrigin(final String origin) throws Exception {
        final LongValueCallbackHelper callbackHelper = new LongValueCallbackHelper();
        final AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();

        int callCount = callbackHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                bridge.getUsageForOrigin(
                                        origin, usage -> callbackHelper.notifyCalled(usage)));
        callbackHelper.waitForCallback(callCount);

        return callbackHelper.getValue();
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testDeleteAll() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        deleteAllData();
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testDeleteOrigin() throws Exception {
        final long initialUsage = getUsageForOrigin(mOrigin);

        AwActivityTestRule.pollInstrumentationThread(
                () -> getUsageForOrigin(mOrigin) > initialUsage);

        deleteOrigin(mOrigin);
        AwActivityTestRule.pollInstrumentationThread(() -> getUsageForOrigin(mOrigin) == 0);
    }

    /*
    @LargeTest
    @Feature({"AndroidWebView", "WebStore"})
    */
    @Test
    @DisabledTest(message = "crbug.com/609980")
    public void testGetResultsMatch() throws Exception {
        AwQuotaManagerBridge bridge =
                mActivityTestRule.getAwBrowserContext().getQuotaManagerBridge();
        AwActivityTestRule.pollInstrumentationThread(
                () -> AwQuotaManagerBridgeTestUtil.getOrigins(bridge).mOrigins.length > 0);

        AwQuotaManagerBridge.Origins origins = AwQuotaManagerBridgeTestUtil.getOrigins(bridge);
        Assert.assertEquals(origins.mOrigins.length, origins.mUsages.length);
        Assert.assertEquals(origins.mOrigins.length, origins.mQuotas.length);

        for (int i = 0; i < origins.mOrigins.length; ++i) {
            Assert.assertEquals(origins.mUsages[i], getUsageForOrigin(origins.mOrigins[i]));
            Assert.assertEquals(origins.mQuotas[i], getQuotaForOrigin());
        }
    }
}
