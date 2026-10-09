// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.test.util.RenderProcessHostUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;

/** Tests for Selected Site Isolation in WebView. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(MULTI_PROCESS)
@DoNotBatch(reason = "Tests manipulate renderer processes and profile isolation state")
@CommandLineFlags.Add({ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"})
public class SelectedSiteIsolationTest {
    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOriginIframeSharesRendererByDefault() throws Throwable {
        String originA = mWebServer.setServerHost("origin-a.test");
        String originB = originA.replace("origin-a.test", "origin-b.test");

        String iframeHtml = "<html><body>Iframe</body></html>";
        String iframeUrl =
                mWebServer.setResponse("/iframe.html", iframeHtml, null).replace(originA, originB);

        String mainHtml = "<html><body><iframe src=\"" + iframeUrl + "\"></iframe></body></html>";
        String mainUrl = mWebServer.setResponse("/main.html", mainHtml, null);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), mainUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RenderFrameHost mainFrame = mAwContents.getWebContents().getMainFrame();
                    List<RenderFrameHost> frames = mainFrame.getAllRenderFrameHosts();
                    Assert.assertEquals(2, frames.size());
                    Assert.assertEquals(mainFrame, frames.get(0));

                    RenderFrameHost iframe = frames.get(1);
                    Assert.assertTrue(mainFrame.isOutermostMainFrame());
                    Assert.assertFalse(iframe.isOutermostMainFrame());
                    Assert.assertEquals(mainUrl, mainFrame.getLastCommittedURL().getSpec());
                    Assert.assertEquals(iframeUrl, iframe.getLastCommittedURL().getSpec());
                    Assert.assertNotEquals(
                            mainFrame.getLastCommittedOrigin(), iframe.getLastCommittedOrigin());

                    int mainFrameProcessId = mainFrame.getGlobalRenderFrameHostId().childId();
                    int iframeProcessId = iframe.getGlobalRenderFrameHostId().childId();
                    Assert.assertEquals(mainFrameProcessId, iframeProcessId);
                });

        Assert.assertEquals(1, RenderProcessHostUtils.getCurrentRenderProcessCount());
    }
}
