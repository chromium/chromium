// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.Process;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.test.util.TrafficStatsTestUtil;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

/** These tests on only supported on O- devices. See crbug.com/374694125 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class TrafficStatsTest extends AwParameterizedTest {

    private static final String TAG = "TrafficStatsTest";
    private static final int TRAFFIC_TAG = 12345678;

    private AwContents mAwContents;
    private AwTestContainerView mTestContainer;
    private TestAwContentsClient mContentsClient;
    private EmbeddedTestServer mTestServer;

    @Rule public AwActivityTestRule mActivityTestRule;

    public TrafficStatsTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        Assume.assumeTrue(
                "Skipping TrafficStatsTest - GetTaggedBytes unsupported.",
                TrafficStatsTestUtil.nativeCanGetTaggedBytes());

        mContentsClient = new TestAwContentsClient();
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testTagging_Once() throws Throwable {
        AwContentsStatics.setDefaultTrafficStatsTag(TRAFFIC_TAG);
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
        long priorBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);

        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        long newBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);
        Assert.assertTrue(
                "expected new bytes:" + newBytes + " to be greater than prior bytes:" + priorBytes,
                newBytes > priorBytes);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testTagging_MultipleTimes_newRequestsUseNewTag() throws Throwable {
        // First tagging and loading
        AwContentsStatics.setDefaultTrafficStatsTag(TRAFFIC_TAG);
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Another tagging and loading
        int another_tag = 87654321;
        long priorBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(another_tag);
        AwContentsStatics.setDefaultTrafficStatsTag(another_tag);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        long newBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(another_tag);
        Assert.assertTrue(
                "expected new bytes:" + newBytes + " to be greater than prior bytes:" + priorBytes,
                newBytes > priorBytes);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testTagging_WithAppUid_taggingStillWorks() throws Throwable {
        AwContentsStatics.setDefaultTrafficStatsUid(Process.myUid());
        AwContentsStatics.setDefaultTrafficStatsTag(TRAFFIC_TAG);
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");

        long priorBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        long newBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);
        Assert.assertTrue(
                "expected new bytes:" + newBytes + " to be greater than prior bytes:" + priorBytes,
                newBytes > priorBytes);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testTagging_WithOtherAppUid_taggingNotAssociatedWithOurAppId() throws Throwable {
        AwContentsStatics.setDefaultTrafficStatsUid(10001);
        AwContentsStatics.setDefaultTrafficStatsTag(TRAFFIC_TAG);
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");

        long priorBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        long newBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(TRAFFIC_TAG);
        Assert.assertEquals("Tagged bytes count should be same", newBytes, priorBytes);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @OnlyRunIn(EITHER_PROCESS) // This test doesn't use the renderer process
    public void testTagging_UnsetTag_SocketIsTaggedWithZero() throws Throwable {
        String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");

        int traffic_tag = 0;
        long priorBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(traffic_tag);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        long newBytes = TrafficStatsTestUtil.nativeGetTaggedBytes(traffic_tag);
        Assert.assertTrue(
                "expected new bytes:" + newBytes + " to be greater than prior bytes:" + priorBytes,
                newBytes > priorBytes);
    }

    // Testing other app uid is currently infeasible due to requiring UPDATE_DEVICE_STATS manifest
    // permission which is currently not granted for 3p apps including test apps. See
    // https://developer.android.com/reference/android/Manifest.permission#UPDATE_DEVICE_STATS
}
